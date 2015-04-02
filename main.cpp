/*
 * Upload (stage) a single large image or directory tree using multiple threads
 */
#include <fstream>
#include <string>
#include <memory>
#include <map>
#include <vector>

#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <exception>

#ifndef LOG_DEBUG
#define LOG_DEBUG std::printf
#endif

class RoamesError : public std::exception
{
public:
    RoamesError(const char *format, ...) : txt(format), std::exception() {}

    virtual const char *what() const noexcept { return txt.c_str(); }
    std::string txt; 
};

class POSHeader
{
public:
    POSHeader() {}
    virtual ~POSHeader() {}

    void read(std::istream &is);

    const size_t getDataSize() const { return m_c_filesize; }
    const size_t getDataOffset() const { return m_dataOffset; }

    const std::string &getName() const { return m_fileName; }

protected:
    static const uint8_t ms_magic[6];   // must be "070707"

    std::string m_fileName;
    uint16_t m_c_dev;          //  6
    uint16_t m_c_ino;          //  6
    uint16_t m_c_mode;         //  6       see below for value
    uint16_t m_c_uid;          //  6
    uint16_t m_c_gid;          //  6
    uint16_t m_c_nlink;        //  6
    uint16_t m_c_rdev;         //  6       only valid for chr and blk special files
    uint32_t m_c_mtime;        //  11
    uint16_t m_c_namesize;     //  6       count includes terminating NUL in pathname
    uint32_t m_c_filesize;     //  11      must be 0 for FIFOs and directories
    size_t m_headerOffset;
    size_t m_dataOffset;
};

const uint8_t POSHeader::ms_magic[6] = {0x30, 0x37, 0x30, 0x37, 0x30, 0x37}; // 070707

class POSFile
{
public:
    POSFile(const std::string &filename) : m_filename(filename)
    {
        if (!filename.empty())
        {
            m_fileStream.open(filename, std::ifstream::in|std::ifstream::binary);
        }
    }
    virtual ~POSFile()
    {
        if (m_fileStream.is_open())
        {
            m_fileStream.close();
        }
    }

    int read()
    {
        if (!m_fileStream.is_open())
        {
            return 0;
        }

        while (m_fileStream)
        {
            try
            {
                POSHeader hdr;
                hdr.read(m_fileStream);
                m_fileStream.seekg(hdr.getDataOffset() + hdr.getDataSize());
                if (hdr.getName() == "TRAILER!!!")
                {
                    break;
                }
                m_headers[hdr.getName()] = hdr;
            }
            catch (std::exception &e)
            {
                std::printf("FAILED: Could not read header");
                break;
            }
        }

        return m_headers.size() < 1 ? 0 : m_headers.size()-1;
    }

    bool hasDescriptionFile() const
    {
        return m_headers.count("description") == 1; // THERE CAN BE ONLY ONE!!!11!1one!!
    }

    const std::stringstream &readDescription(std::stringstream &ss) const;

private:
    typedef std::map<std::string, POSHeader> HeaderMap;

    HeaderMap m_headers;
    std::string m_filename;
    mutable std::ifstream m_fileStream;
};

class GroupData
{
public:
    enum eDataTypes
    {
        eDT_UNK = 0,
        eDT_INT,
        eDT_UINT,
        eDT_LONG,
        eDT_ULONG
    };

    GroupData(const eDataTypes type, const size_t sz) : m_type(type), m_size(sz)
    {
    }
    GroupData(const GroupData &orig)
    {
        copyData(orig);
    }
    ~GroupData()
    {
    }
    const GroupData &operator=(const GroupData &rhs)
    {
        copyData(rhs);
        return *this;
    }
private:
    void copyData(const GroupData &rhs)
    {
        m_type = rhs.m_type;
        m_size = rhs.m_size;
    }
    eDataTypes m_type;
    size_t m_size;
};

class GroupFile
{
public:
    GroupFile() {}
    ~GroupFile() {}

private:
    typedef std::map<std::string, GroupData> DataMap;

    DataMap m_dataMap;  // maps variable names to data
};

typedef std::vector<GroupFile> GoupFileVector;

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cerr << "Error: not enough args" << std::endl << std::endl;
        return 1;
    }

    std::string inFile = argv[1];
    std::string outFile = argv[2];

    std::ifstream is(inFile, std::ifstream::in | std::ifstream::binary);
    if (!is.is_open())
    {
        std::cerr << "Could not open file \"" << inFile << "\"" << std::endl;
        return 1;
    }
    is.close();

    try
    {
        POSFile fl(inFile);
        fl.read();
        std::stringstream descBuffer;
        fl.readDescription(descBuffer);
        std::cout << std::endl
                  << "Description" << std::endl
                  << "-----------" << std::endl
                  << descBuffer.rdbuf() // should not need an endl
                  << "-----------" << std::endl << std::endl;

        return 0;

    }
    catch (RoamesError &e)
    {
        std::cerr << "RoamesError while reading POS file [" << e.what() << "]" << std::endl;
    }
    catch (std::exception &e)
    {
        std::cerr << "std::exception while reading POS file [" << e.what() << "]" << std::endl;
    }
    catch(...)
    {
        std::cerr << "Unknown exception reading POS file" << std::endl;
    }

    return 1;
}

#define READOCT(tval, is, var, buf, sz)                 \
    do {                                                \
        LOG_DEBUG(#var": ");                            \
        readOctBuffer<tval>(is, buf, sz, var);          \
    } while(0);

template <typename T>
void readOctBuffer(std::istream & is, char * buf, const size_t bufSize, T &value)
{
    if (!is)
    {
        throw RoamesError("Invalid stream");
    }
    if (!buf || bufSize <= 6)    // HACK :(
    {
        throw RoamesError("Invalid Buffer");
    }
    size_t readSize = bufSize-1;
    is.read(buf, readSize);
    if (!is || (is.gcount() != readSize))
    {
        throw RoamesError("Could not read %u byte buffer", readSize);
    }
    else
    {
        value = 0;
        std::stringstream ss(buf);
        ss.setf(std::ios::oct, std::ios::basefield);
        ss >> value;

        LOG_DEBUG("Read string \"%s\" value = %u\n", buf, value);
    }
}

void POSHeader::read(std::istream & is)
{
    if (!is)
    {
        throw RoamesError("Invalid input stream");
    }

    char small_buf[7] = {0};        // 6 chars and null
    char large_buf[12] = {0};       // 11 chars and null

    memset(small_buf, 0, 7);
    memset(large_buf, 0, 12);

    m_headerOffset = is.tellg();

    // read the header
    // first check the magic value
    is.read(small_buf, 6);
    if (!is || (is.gcount() != 6))
    {
        throw RoamesError("Failed to read small buffer");
    }
    if (memcmp(small_buf, ms_magic, 6) != 0)
    {
        throw RoamesError("Could not read CPIO header magic value");
    }

    READOCT(uint16_t, is, m_c_dev, small_buf, 7);
    READOCT(uint16_t, is, m_c_ino, small_buf, 7);
    READOCT(uint16_t, is, m_c_mode, small_buf, 7);
    READOCT(uint16_t, is, m_c_uid, small_buf, 7);
    READOCT(uint16_t, is, m_c_gid, small_buf, 7);
    READOCT(uint16_t, is, m_c_nlink, small_buf, 7);
    READOCT(uint16_t, is, m_c_rdev, small_buf, 7);
    READOCT(uint32_t, is, m_c_mtime, large_buf, 12);
    READOCT(uint16_t, is, m_c_namesize, small_buf, 7);
    READOCT(uint32_t, is, m_c_filesize, large_buf, 12);

    if (m_c_namesize >= 1)
    {
        char nameBuf[1024];
        memset(nameBuf, 0, 1024);
        is.read(nameBuf, m_c_namesize);
        if (!is || (is.gcount() != m_c_namesize))
        {
            throw RoamesError("Could not read file name");
        }
        m_fileName = nameBuf;
        LOG_DEBUG("Filename = \"%s\"\n", m_fileName.c_str());
    }
    else
    {
        throw RoamesError("Invalid name size");
    }

    m_dataOffset = m_headerOffset + 76 + m_c_namesize;
    LOG_DEBUG("Offsets: start = %lu, data = %lu\n\n", m_headerOffset, m_dataOffset);
}

const std::stringstream &POSFile::readDescription(std::stringstream &ss) const
{
    if (!hasDescriptionFile())
    {
        throw RoamesError("No description file found");
    }
    HeaderMap::const_iterator itr = m_headers.find("description");
    const POSHeader &hdr = itr->second;

    if (!m_fileStream.good())
        throw RoamesError("Invalid filestream");
    m_fileStream.seekg(hdr.getDataOffset());
    if (!m_fileStream.good())
        throw RoamesError("Failed to seek to begining of description data");
    // the description data is a simple text file so we should easily fit it in memory
    std::unique_ptr<char> buf(new char[hdr.getDataSize()]);
    m_fileStream.read(buf.get(), hdr.getDataSize());
    if (!m_fileStream.good())
        throw RoamesError("Failed to read description data");

    ss << buf.get();
    return ss;
}
