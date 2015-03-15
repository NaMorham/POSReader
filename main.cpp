/*
 * Upload (stage) a single large image or directory tree using multiple threads
 */
#include <fstream>
#include <string>
#include <memory>
#include <map>

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

private:
    typedef std::map<std::string, POSHeader> HeaderMap;

    HeaderMap m_headers;
    std::string m_filename;
    std::ifstream m_fileStream;
};

int main(int argc, char *argv[])
{
    std::printf("DOOOOM\n\n");

    if (argc < 3)
    {
        std::printf("Error: not enough args\n\n");
        return 1;
    }

    std::string inFile = argv[1];
    std::string outFile = argv[2];

    std::ifstream is(inFile, std::ifstream::in | std::ifstream::binary);
    if (!is.is_open())
    {
        std::printf("Could not open file \"%s\"\n\n", inFile.c_str());
        return 1;
    }

    /*
    POSHeader hdr, hdr2, hdr3;
    hdr.read(is);
    is.seekg(hdr.getDataOffset() + hdr.getDataSize());
    hdr2.read(is);
    is.seekg(hdr2.getDataOffset() + hdr2.getDataSize());
    hdr3.read(is);
    /*/
    POSFile fl(inFile);
    fl.read();
    //*/
	is.close();

	return 0;
}

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

    char small_buf[7] = {0};    // 6 chars and null
    char large_buf[12] = {0};    // 11 chars and null

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

    readOctBuffer<uint16_t>(is, small_buf, 7, m_c_dev);
    readOctBuffer<uint16_t>(is, small_buf, 7, m_c_ino);
    readOctBuffer<uint16_t>(is, small_buf, 7, m_c_mode);
    readOctBuffer<uint16_t>(is, small_buf, 7, m_c_uid);
    readOctBuffer<uint16_t>(is, small_buf, 7, m_c_gid);
    readOctBuffer<uint16_t>(is, small_buf, 7, m_c_nlink);
    readOctBuffer<uint16_t>(is, small_buf, 7, m_c_rdev);
    readOctBuffer<uint32_t>(is, large_buf, 12, m_c_mtime);
    readOctBuffer<uint16_t>(is, small_buf, 7, m_c_namesize);
    readOctBuffer<uint32_t>(is, large_buf, 12, m_c_filesize);

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
    LOG_DEBUG("Offsets: start = %lu, data = %lu\n", m_headerOffset, m_dataOffset);
}



