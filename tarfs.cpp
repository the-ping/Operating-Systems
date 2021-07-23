/*
 * TAR File-system Driver
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (4)
 */

/*
 * STUDENT NUMBER: s1770036
 */
#include "tarfs.h"
#include <infos/kernel/log.h>

using namespace infos::fs;
using namespace infos::drivers;
using namespace infos::drivers::block;
using namespace infos::kernel;
using namespace infos::util;
using namespace tarfs;


/**
 * TAR files contain header data encoded as octal values in ASCII.  This function
 * converts this terrible representation into a real unsigned integer.
 *
 * You DO NOT need to modify this function.
 *
 * @param data The (null-terminated) ASCII data containing an octal number.
 * @return Returns an unsigned integer number, corresponding to the input data.
 */
static inline unsigned int octal2ui(const char *data)
{
	// Current working value.
	unsigned int value = 0;

	// Length of the input data.
	int len = strlen(data);

	// Starting at i = 1, with a factor of one.
	int i = 1, factor = 1;
	while (i < len) {
		// Extract the current character we're working on (backwards from the end).
		char ch = data[len - i];

		// Add the value of the character, multipled by the factor, to
		// the working value.
		value += factor * (ch - '0');

		// Increment the factor by multiplying it by eight.
		factor *= 8;

		// Increment the current character position.
		i++;
	}

	// Return the current working value.
	return value;
}

// The structure that represents the header block present in
// TAR files.  A header block occurs before every file, this
// this structure must EXACTLY match the layout as described
// in the TAR file format description.
namespace tarfs {
	struct posix_header {
		char name[100]; // ASCII (+ Z unless filled)
		char mode[8];
		char uid[8];
		char gid[8];
		char size[12]; // 0 padded, octal, null
		char mtime[12];
		char chksum[8];
		char typeflag;
		char linkname[100];
		char magic[6];
		char version[2];
		char uname[32];
		char gname[32];
		char devmajor[8];
		char devminor[8];
		char prefix[155];

	} __packed;
}


/**
 * Reads the contents of the file into the buffer, from the specified file offset.
 * @param buffer The buffer to read the data into.
 * @param size The size of the buffer, and hence the number of bytes to read.
 * @param off The offset within the file.
 * @return Returns the number of bytes read into the buffer.
 */
int TarFSFile::pread(void* buffer, size_t size, off_t off)
{
	// if offset is more than file size
	if (off >= this->size()) {
		return 0;
	}
	// if there's no buffer size or if size file is 0
	if (size == 0 || this->size() == 0) {
		return 0;
	}


	// no. of bytes to be read including the offset
	int offset_size = off % 512;
	// the offset but in blocks
	int offset_block = off / 512;
	// bytes to be read
	int read_bytes = size + offset_size;
	int read_blocks;
	// the starting point of the file to read from
	int starting_block = _file_start_block + offset_block;

	if(this->size() < (size+off)) {
		read_bytes = this->size() - offset_block;
	}

	// calculate the blocks to be read
	if(read_bytes % 512 == 0) {
		read_blocks = read_bytes/512;
	} else { // round it up if there's extra space
		read_blocks = (read_bytes/512) + 1;
	}

	// put into temp buffer
	uint8_t *temp_buffer = new uint8_t[512 * read_blocks];
	_owner.block_device().read_blocks(temp_buffer, starting_block, read_blocks);

	// then transfer into the given buffer
	for (int byte = 0; byte < (read_bytes-offset_size); byte++ ) {
		*((char *) buffer + byte) = temp_buffer[byte + offset_size];
	}

	delete temp_buffer;
	return (read_bytes - offset_size);

}

/**
 * Reads all the file headers in the TAR file, and builds an in-memory
 * representation.
 * @return Returns the root TarFSNode that corresponds to the TAR file structure.
 */
TarFSNode* TarFS::build_tree()
{
	// Create the root node.
	TarFSNode *root = new TarFSNode(NULL, "", *this); //declared in line 103 of tarfs.h. Says: no parent node, empty name string, *this is the owner??
	TarFSNode *node = root; // this node
	size_t nr_blocks = block_device().block_count();
	unsigned int block_size = 512;
	// Create header object
	struct posix_header *header;
	header = (struct posix_header *) new char[block_device().block_size()];
	List <String> path_list;
	String hdr_name;
	unsigned int file_size;
	String file_name;
	String file_path;

	// iterate through the block_device
	for (size_t idx = 0; idx < nr_blocks; idx += file_size+1) {

		// Read the a block into the header structure.
		block_device().read_blocks(header, idx, 1);

		// if zero block exists, it's the end of the archive
		if (is_zero_block((uint8_t *) header)) {
            uint8_t *block_zero = new uint8_t[block_device().block_size()];
            block_device().read_blocks(block_zero, idx + 1, 1);

						// if this block is zero, delete and breka the loop
            if (is_zero_block(block_zero)) {
                delete(block_zero);
                break;
            }
            delete(block_zero);
        }

				// split full path into list of components, and update current file_name
				file_path = String(header->name);
				List <String> split_path_parts = file_path.split('/', true);

				// if it isn't empty, the current file name would be its last item of the list
				if (!split_path_parts.empty()) {
					file_name = split_path_parts.last();
				}
				else {
					file_name = file_path;
				}


				// check if the file is on lower level than the current file
				if (split_path_parts.count() > 1) {
					String dir = split_path_parts.at(split_path_parts.count() - 2);
					String node_name = (String) node->name();

					while (infos::util::strncmp(node_name.c_str(), dir.c_str(), node_name.length()) && (node_name.length() != dir.length())) {
							node = (TarFSNode *) node->parent(); //convert to TarFSNode!
							node_name = (String) node->name();
					}
				}// if the file is on the same level as current file
				else  if (split_path_parts.count() == 1) {
					node = root;
				}

				// update the size of the file if there's extra space
				file_size = octal2ui(header->size);
				file_size = (file_size % block_size) ? (file_size/ block_size + 1) : file_size / block_size;

				// Add child
			 TarFSNode *current_node = new TarFSNode(node, file_name, *this);
			 current_node->set_block_offset(idx);
			 current_node->size(file_size);
			 node->add_child(file_name, current_node);

			 if (file_path.c_str()[file_path.length() - 1] == '/') {
					 // increment the node if the last item in the path is '/'
					 node = current_node;
			 }

	}
	return root;
}

/**
 * Returns the size of this TarFS File
 */
unsigned int TarFSFile::size() const
{
	return octal2ui(_hdr->size);
}

/* --- YOU DO NOT NEED TO CHANGE ANYTHING BELOW THIS LINE --- */

/**
 * Mounts a TARFS filesystem, by pre-building the file system tree in memory.
 * @return Returns the root node of the TARFS filesystem.
 */
PFSNode *TarFS::mount()
{
	// If the root node has not been generated, then build it.
	if (_root_node == NULL) {
		_root_node = build_tree();
	}

	// Return the root node.
	return _root_node;
}

/**
 * Constructs a TarFS File object, given the owning file system and the block
 */
TarFSFile::TarFSFile(TarFS& owner, unsigned int file_header_block)
: _hdr(NULL),
_owner(owner),
_file_start_block(file_header_block),
_cur_pos(0)
{
	// Allocate storage for the header.
	_hdr = (struct posix_header *) new char[_owner.block_device().block_size()]; //creates a header block of size 512bytes

	// Read the header block into the header structure.
	_owner.block_device().read_blocks(_hdr, _file_start_block, 1);

	// Increment the starting block for file data.
	_file_start_block++;
}

TarFSFile::~TarFSFile()
{
	// Delete the header structure that was allocated in the constructor.
	delete _hdr;
}

/**
 * Releases any resources associated with this file.
 */
void TarFSFile::close()
{
	// Nothing to release.
}

/**
 * Reads the contents of the file into the buffer, from the current file offset.
 * The current file offset is advanced by the number of bytes read.
 * @param buffer The buffer to read the data into.
 * @param size The size of the buffer, and hence the number of bytes to read.
 * @return Returns the number of bytes read into the buffer.
 */
int TarFSFile::read(void* buffer, size_t size)
{
	// Read can be seen as a special case of pread, that uses an internal
	// current position indicator, so just delegate actual processing to
	// pread, and update internal state accordingly.

	// Perform the read from the current file position.
	int rc = pread(buffer, size, _cur_pos);

	// Increment the current file position by the number of bytes that was read.
	// The number of bytes actually read may be less than 'size', so it's important
	// we only advance the current position by the actual number of bytes read.
	_cur_pos += rc;

	// Return the number of bytes read.
	return rc;
}

/**
 * Moves the current file pointer, based on the input arguments.
 * @param offset The offset to move the file pointer either 'to' or 'by', depending
 * on the value of type.
 * @param type The type of movement to make.  An absolute movement moves the
 * current file pointer directly to 'offset'.  A relative movement increments
 * the file pointer by 'offset' amount.
 */
void TarFSFile::seek(off_t offset, SeekType type)
{
	// If this is an absolute seek, then set the current file position
	// to the given offset (subject to the file size).  There should
	// probably be a way to return an error if the offset was out of bounds.
	if (type == File::SeekAbsolute) {
		_cur_pos = offset;
	} else if (type == File::SeekRelative) {
		_cur_pos += offset;
	}
	if (_cur_pos >= size()) {
		_cur_pos = size() - 1;
	}
}

TarFSNode::TarFSNode(TarFSNode *parent, const String& name, TarFS& owner) : PFSNode(parent, owner), _name(name), _size(0), _has_block_offset(false), _block_offset(0)
{
}

TarFSNode::~TarFSNode()
{
}

/**
 * Opens this node for file operations.
 * @return
 */
File* TarFSNode::open()
{
	// This is only a file if it has been associated with a block offset.
	if (!_has_block_offset) {
		return NULL;
	}

	// Create a new file object, with a header from this node's block offset.
	return new TarFSFile((TarFS&) owner(), _block_offset);
}

/**
 * Opens this node for directory operations.
 * @return
 */
Directory* TarFSNode::opendir()
{
	return new TarFSDirectory(*this);
}

/**
 * Attempts to retrieve a child node of the given name.
 * @param name
 * @return
 */
PFSNode* TarFSNode::get_child(const String& name)
{
	TarFSNode *child;

	// Try to find the given child node in the children map, and return
	// NULL if it wasn't found.
	if (!_children.try_get_value(name.get_hash(), child)) {
		return NULL;
	}

	return child;
}

/**
 * Creates a subdirectory in this node.  This is a read-only file-system,
 * and so this routine does not need to be implemented.
 * @param name
 * @return
 */
PFSNode* TarFSNode::mkdir(const String& name)
{
	// DO NOT IMPLEMENT
	return NULL;
}

/**
 * A helper routine that updates this node with the offset of the block
 * that contains the header of the file that this node represents.
 * @param offset The block offset that corresponds to this node.
 */
void TarFSNode::set_block_offset(unsigned int offset)
{
	_has_block_offset = true;
	_block_offset = offset;
}

/**
 * A helper routine that adds a child node to the internal children
 * map of this node.
 * @param name The name of the child node.
 * @param child The actual child node.
 */
void TarFSNode::add_child(const String& name, TarFSNode *child)
{
	_children.add(name.get_hash(), child);
}

TarFSDirectory::TarFSDirectory(TarFSNode& node) : _entries(NULL), _nr_entries(0), _cur_entry(0)
{
	_nr_entries = node.children().count();
	_entries = new DirectoryEntry[_nr_entries];

	int i = 0;
	for (const auto& child : node.children()) {
		_entries[i].name = child.value->name();
		_entries[i++].size = child.value->size();
	}
}

TarFSDirectory::~TarFSDirectory()
{
	delete _entries;
}

bool TarFSDirectory::read_entry(infos::fs::DirectoryEntry& entry)
{
	if (_cur_entry < _nr_entries) {
		entry = _entries[_cur_entry++];
		return true;
	} else {
		return false;
	}
}

void TarFSDirectory::close()
{

}

static Filesystem *tarfs_create(VirtualFilesystem& vfs, Device *dev)
{
	if (!dev->device_class().is(BlockDevice::BlockDeviceClass)) return NULL;
	return new TarFS((BlockDevice &) * dev);
}

RegisterFilesystem(tarfs, tarfs_create);
