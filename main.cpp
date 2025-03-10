// written by Paul Baxter
#include <iostream>
#include <sstream>
#include <vector>
#include <fstream>
#include <cstring>

#include "argparse/argparse.hpp"

#include "d64.h"

enum ComformationType {
    overwrite_file,
    skip_file,
    overwrite_all,
    skip_all
};

ComformationType conformation = overwrite_file;

std::string diskname;
char backup_disk_num = '0';
std::string target_backup_base_name;

bool fileExists(d64& disk, const std::string& filename);
void Backup(const std::string& source, const std::string& target);
bool copyFiles(d64& sourceDisk, d64& targetDisk);

void handleHelp();
void handleCreate(const std::string& diskfile, bool fortyTracks);
void handleBAM(const std::string& diskfile);
void handleDumpSector(const std::string& diskfile, int track, int sector);
void handleAdd(const std::string& diskfile, const std::string& filename);
void handleAddRel(const std::string& diskfile, const std::string& filename, const int recordsize);
void handleList(const std::string& diskfile);
void handleLock(const std::string& diskfile, const std::string& filename);
void handleLoad(const std::string& diskfile);
void handleUnlock(const std::string& diskfile, const std::string& filename);
void handleExtract(const std::string& diskfile, const std::string& filename);
void handleRemove(const std::string& diskfile, const std::string& filename);
void handleRename(const std::string& diskfile, const std::string& oldname, const std::string& newname);
void handleVerify(const std::string& diskfile, bool fix);
void handleCompact(const std::string& diskfile);
void handleReorder(const std::string& diskfile, const std::vector<std::string>& order);
void handleDiskRename(const std::string& diskfile, const std::string& newname);
void handleBackup(const std::string& diskfile, const std::vector<std::string>& order);

void interactiveShell();

typedef enum {
    no_param,
    one_param,
    two_param,
    three_param,
    two_bool,
    file_list,
    two_int
} funcType;

typedef void (*fun0)();
typedef void (*fun1)(const std::string& param1);
typedef void (*fun2)(const std::string& param1, const std::string& param2);
typedef void (*fun3)(const std::string& param1, const std::string& param2, const std::string& param3);
typedef void (*funB)(const std::string& param1, bool);
typedef void (*funN)(const std::string& diskfile, const std::vector<std::string>& order);
typedef void (*funI)(const std::string& diskfile, const int track, const int sector);

struct InteractiveFunctions {
    funcType type;
    union {
        fun0 f0;
        fun1 f1;
        fun2 f2;
        fun3 f3;
        funB fb;
        funN fn;
        funI fi;
    };
};

std::map<std::string, InteractiveFunctions> functionTable = {
    {"help", {no_param, {.f0 = handleHelp}}},
    {"--help", {no_param, {.f0 = handleHelp}}},
    {"--h", {no_param, {.f0 = handleHelp}}},
    {"create", {two_bool, {.fb = handleCreate}}},
    {"format", {two_bool, {.fb = handleCreate}}},
    {"list", {one_param, {.f1 = handleList}}},
    {"dir", {one_param, {.f1 = handleList}}},
    {"load", {one_param, {.f1 = handleLoad}}},
    {"add", {two_param, {.f2 = handleAdd}}},
    {"extract", {two_param, {.f2 = handleExtract}}},
    {"remove", {two_param, {.f2 = handleRemove}}},
    {"del", {two_param, {.f2 = handleRemove}}},
    {"rename", {three_param, {.f3 = handleRename}}},
    {"rename-disk", {two_param, {.f2 = handleDiskRename}}},
    {"bam", {one_param, {.f1 = handleBAM}}},
    {"verify", {two_bool, {.fb = handleVerify}}},
    {"compact", {one_param, {.f1 = handleCompact}}},
    {"reorder", {file_list, {.fn = handleReorder}}},
    {"backup", {file_list, {.fn = handleBackup}}},
    {"lock", {two_param, {.f2 = handleLock}}},
    {"unlock", {two_param, {.f2 = handleUnlock}}},
    {"dump", {two_int, {.fi = handleDumpSector}}},
    { "load", {one_param, {.f1 = handleLoad} }}
    };

argparse::ArgumentParser program("d64");

/// <summary>
/// Handle help command
/// </summary>
void handleHelp()
{
    std::cout << program << "\n";
}

/// <summary>
/// Load a d64 image
/// </summary>
/// <param name="filename">name of file</param>
void handleLoad(const std::string& diskfile)
{
    diskname = diskfile;
    d64 disk;
    if (disk.load(diskname)) {
        std::cout << "Loaded disk: " << diskname << "\n";
    }
    else {
        std::cerr << "Error: Failed to load disk.\n";
        diskname.clear();
    }
}

/// <summary>
/// Create a d64 image
/// </summary>
/// <param name="filename">name of file</param>
void handleCreate(const std::string& diskfile, bool forty_tracks)
{
    diskname = diskfile;

    auto disktype = forty_tracks ? diskType::forty_track : diskType::thirty_five_track;
    d64 disk(disktype);
    disk.formatDisk("NEW DISK");
    if (disk.save(diskname)) {
        std::cout << "Created new disk: " << diskname << "\n";
    }
    else {
        std::cerr << "Error: Failed to create disk.\n";
        diskname.clear();
    }
}

/// <summary>
/// Display d64 BAM
/// </summary>
/// <param name="filename">name of file</param>
void handleBAM(const std::string& diskfile)
{
    d64 disk;
    diskname = diskfile;

    if (disk.load(diskname)) {
        for (auto track = 1; track <= disk.TRACKS; ++track) {
            std::cout << std::setw(4) << track << ' ';

            for (auto sector = 0; sector < disk.SECTORS_PER_TRACK[track - 1]; ++sector) {
                auto free = disk.bamtrack(track - 1)->test(sector);
                auto ch = free ? '.' : '*';
                std::cout << std::setw(0) << ch;
            }
            std::cout << '\n';
        }
    }
    else {
        std::cerr << "Error: Could not load disk.\n";
        diskname.clear();
    }
}

/// <summary>
/// Add a file to a d64 disk image
/// </summary>
/// <param name="diskfile">diskfile to use</param>
/// <param name="filename">file to add</param>
void handleAdd(const std::string& diskfile, const std::string& filename)
{
    d64 disk;
    diskname = diskfile;

    // open the disk file
    if (disk.load(diskname)) {
        std::ifstream fs(filename, std::ios::binary);
        if (!fs.is_open()) {
            std::cerr << "Error: unable to open file " << filename << ".\n";
            return;
        }
        fs.seekg(0, std::ios::end);
        auto length = fs.tellg();

        fs.seekg(0, std::ios::beg);
        std::vector<uint8_t> fileData(length);
        fs.read((char*)&fileData[0], length);
        fs.close();

        // get the name part of the filename
        // convert to upper case and remove extension 
        auto name = filename;
        auto index = static_cast<int>(name.size()) - 1;
        auto endindex = 0;
        while (index >= 0) {
            name[index] = toupper(name[index]);
            if (endindex == 0) {
                if (name[index] == '.') {
                    endindex = index--;
                    continue;
                }
            }
            if (ispunct(name[index])) {
                break;
            }
            --index;
        }

        FileTypes filetype;
        if (name.ends_with(".PRG"))
            filetype = FileTypes::PRG;
        else if (name.ends_with(".SEQ"))
            filetype = FileTypes::SEQ;
        else if (name.ends_with(".USR"))
            filetype = FileTypes::USR;
        else if (name.ends_with(".REL")) {
            std::cerr << "Error: Use addrel to add .rel files.\n";
            return;
        }
        else {
            std::cerr << "Error: Unknown file type. Using .PRG.\n";
            filetype = FileTypes::PRG;
        }
        
        name = (endindex == 0) ? name.substr(index + 1) : name.substr(index + 1, (endindex - 1) - index);
        if (disk.addFile(name, filetype, fileData)) {
            disk.save(diskfile);
            std::cout << "Added file: " << filename << " to " << disk.diskname() << "\n";
        }
        else {
            std::cerr << "Error: Failed to add file.\n";
            diskname.clear();
        }
    }
    else {
        std::cerr << "Error: Could not load disk.\n";
        diskname.clear();
    }
}

/// <summary>
/// Add a .rel file to a d64 disk image
/// </summary>
/// <param name="diskfile">diskfile to use</param>
/// <param name="filename">file to add</param>
/// <param name="recordsize">record sie</param>
void handleAddRel(const std::string& diskfile, const std::string& filename, const int recordsize)
{
    d64 disk;
    diskname = diskfile;

    // open the disk file
    if (disk.load(diskname)) {
        std::ifstream fs(filename, std::ios::binary);
        if (!fs.is_open()) {
            std::cerr << "Error: unable to open file " << filename << ".\n";
            return;
        }
        fs.seekg(0, std::ios::end);
        auto length = fs.tellg();

        fs.seekg(0, std::ios::beg);
        std::vector<uint8_t> fileData(length);
        fs.read((char*)&fileData[0], length);
        fs.close();

        // get the name part of the filename
        // convert to upper case and remove extension 
        auto name = filename;
        auto index = static_cast<int>(name.size()) - 1;
        auto endindex = 0;
        while (index >= 0) {
            name[index] = toupper(name[index]);
            if (endindex == 0) {
                if (name[index] == '.') {
                    endindex = index--;
                    continue;
                }
            }
            if (ispunct(name[index])) {
                break;
            }
            --index;
        }

        auto filetype = FileTypes::REL;

        name = (endindex == 0) ? name.substr(index + 1) : name.substr(index + 1, (endindex - 1) - index);
        if (disk.addRelFile(name, filetype, recordsize, fileData)) {
            disk.save(diskfile);
            std::cout << "Added file: " << filename << " to " << disk.diskname() << "\n";
        }
        else {
            std::cerr << "Error: Failed to add file.\n";
            diskname.clear();
        }
    }
    else {
        std::cerr << "Error: Could not load disk.\n";
        diskname.clear();
    }
}

/// <summary>
/// List files on the a d64 disk image
/// </summary>
/// <param name="diskfile">diskfile to use</param>
void handleList(const std::string& diskfile)
{
    d64 disk;
    diskname = diskfile;

    if (disk.load(diskname)) {
        std::cout << "Directory of " << disk.diskname() << "\n";
        std::cout << disk.getFreeSectorCount() << " free sectors\n";
        for (const auto& entry : disk.directory()) {
            std::cout << std::setw(15) << d64::Trim(entry.file_name) << (entry.file_type.locked ? "< " : "  ");

            uint8_t type = entry.file_type.type;
            switch (type) {
                case FileTypes::PRG:
                    std::cout << "PRG";
                    break;
                case FileTypes::SEQ:
                    std::cout << "SEQ";
                    break;
                case FileTypes::USR:
                    std::cout << "USR";
                    break;
                case FileTypes::REL:
                    std::cout << "REL";
                    break;
                case FileTypes::DEL:
                    std::cout << "DEL";
                    break;
                default:
                    std::cout << "???";
            }
            std::cout << " " << entry.file_size[0] + entry.file_size[1] * 256 << " sectors\n";
        }
    }
    else {
        std::cerr << "Error: Could not load disk.\n";
        diskname.clear();
    }
}

/// <summary>
/// Lock a file on a d64 disk image
/// </summary>
/// <param name="diskfile">diskfile to use</param>
/// <param name="filename">file to lock</param>
void handleLock(const std::string& diskfile, const std::string& filename)
{
    d64 disk;
    diskname = diskfile;

    if (disk.load(diskname)) {
        if (disk.lockfile(filename, true)) {
            disk.save(diskfile);
            std::cout << "Locked file: " << filename << " from " << disk.diskname() << "\n";
        }
        else {
            std::cerr << "Error: Could not lock file.\n";
        }
    }
    else {
        std::cerr << "Error: Could not load disk.\n";
        diskname.clear();
    }
}

/// <summary>
/// Unlock a file on a d64 disk image
/// </summary>
/// <param name="diskfile">diskfile to use</param>
/// <param name="filename">file to extract</param>
void handleUnlock(const std::string& diskfile, const std::string& filename)
{
    d64 disk;
    diskname = diskfile;

    if (disk.load(diskname)) {
        if (disk.lockfile(filename, true)) {
            disk.save(diskfile);
            std::cout << "Unlocked file: " << filename << " from " << disk.diskname() << "\n";
        }
        else {
            std::cerr << "Error: Could not unlock file.\n";
        }
    }
    else {
        std::cerr << "Error: Could not load disk.\n";
        diskname.clear();
    }
}

/// <summary>
/// Display a sector
/// </summary>
/// <param name="diskfile">diskfile to use</param>
/// <param name="track">track to use</param>
/// <param name="sector">sector to use</param>
void handleDumpSector(const std::string& diskfile, int track, int sector)
{
    d64 disk;
    diskname = diskfile;

    if (disk.load(diskname)) {
        auto data = disk.readSector(track, sector);
        if (data.has_value()) {
            std::cout << "TRACK " << track << " SECTOR " << sector << '\n';
            auto b = 0;
            std::string ascii;
            for (auto& byte : data.value()) {
                if (b % 16 == 0) {
                    std::cout << std::setw(10) << std::setfill(' ') << ' ' << ascii << "\n";
                    ascii.clear();
                }
                ascii += isprint(byte) ? static_cast<char>(byte) : '.';
                std::cout << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(byte) << ' ';
                b++;
            }
            std::cout << std::setw(10) << std::setfill(' ') << ' ' << ascii << "\n" <<
                std::setw(0) << std::dec << std::setfill(' ');
        }
        else {
            std::cerr << "Error: Could not read track " << track << " sector " << sector << ".\n";
        }
    }
    else {
        std::cerr << "Error: Could not load disk.\n";
        diskname.clear();
    }
}

/// <summary>
/// Extract a file from a d64 disk image
/// </summary>
/// <param name="diskfile">diskfile to use</param>
/// <param name="filename">file to extract</param>
void handleExtract(const std::string& diskfile, const std::string& filename)
{
    d64 disk;
    diskname = diskfile;

    if (disk.load(diskname)) {
        if (disk.extractFile(filename)) {
            std::cout << "Extracted file: " << filename << " from " << disk.diskname() << "\n";
        }
        else {
            std::cerr << "Error: Could not extract file.\n";
        }
    }
    else {
        std::cerr << "Error: Could not load disk.\n";
        diskname.clear();
    }
}

/// <summary>
/// Remove a file from a d64 disk image
/// </summary>
/// <param name="diskfile">diskfile to use</param>
/// <param name="filename">file to remove</param>
void handleRemove(const std::string& diskfile, const std::string& filename)
{
    d64 disk;
    diskname = diskfile;

    if (disk.load(diskname)) {
        if (disk.removeFile(filename)) {
            disk.save(diskfile);
            std::cout << "Removed file: " << filename << " from " << disk.diskname() << "\n";
        }
        else {
            std::cerr << "Error: Could not remove file.\n";
        }
    }
    else {
        std::cerr << "Error: Could not load disk.\n";
        diskname.clear();
    } 
}

/// <summary>
/// Rename a file on a d64 disk image
/// </summary>
/// <param name="diskfile">diskfile to use</param>
/// <param name="oldname">file to rename</param>
/// <param name="newname">new name of file</param>
void handleRename(const std::string& diskfile, const std::string& oldname, const std::string& newname)
{
    d64 disk;
    diskname = diskfile;

    if (disk.load(diskname)) {
        if (disk.renameFile(oldname, newname)) {
            disk.save(diskfile);
            std::cout << "Renamed file: " << oldname << " => " << newname << "\n";
        }
        else {
            std::cerr << "Error: Could not rename file.\n";
        }
    }
    else {
        std::cerr << "Error: Could not load disk.\n";
        diskname.clear();
    }
}

/// <summary>
/// Verify a disk and optionally fix it
/// </summary>
/// <param name="diskfile">diskfile to use</param>
/// <param name="fix">true to auto fix errors</param>
void handleVerify(const std::string& diskfile, bool fix)
{
    d64 disk;
    diskname = diskfile;

    if (disk.load(diskname)) {
        bool valid = disk.verifyBAMIntegrity(fix, "");
        if (valid) {
            std::cout << "BAM integrity check passed.\n";
        }
        else {
            std::cerr << "Errors found in BAM.\n";
        }
        if (fix) disk.save(diskfile);
    }
    else {
        std::cerr << "Error: Could not load disk.\n";
        diskname.clear();
    }
}

/// <summary>
/// compact a disk
/// </summary>
/// <param name="diskfile">diskfile to use</param>
void handleCompact(const std::string& diskfile)
{
    d64 disk;
    diskname = diskfile;

    if (disk.load(diskname)) {
        if (disk.compactDirectory()) {
            disk.save(diskfile);
            std::cout << "Compacted directory.\n";
        }
        else {
            std::cerr << "Error: Directory compaction failed.\n";
        }
    }
    else {
        std::cerr << "Error: Could not load disk.\n";
        diskname.clear();
    }
}

/// <summary>
/// Reorder the files on a disk
/// </summary>
/// <param name="diskfile">diskfile to use</param>
/// <param name="order">order of new files. If there are files not on the list they are put at the end</param>
void handleReorder(const std::string& diskfile, const std::vector<std::string>& order)
{
    if (program.is_used("--orderfile")) {
        std::ifstream orderFile(program.get<std::string>("--orderfile"));
        std::vector<std::string> fileOrder;
        std::string line;
        while (std::getline(orderFile, line)) {
            fileOrder.push_back(line);
        }
        handleReorder(program.get<std::string>("diskfile"), fileOrder);
    }

    d64 disk;
    diskname = diskfile;

    if (disk.load(diskname)) {
        if (disk.reorderDirectory(order)) {
            disk.save(diskfile);
            std::cout << "Reordered files on disk.\n";
        }
        else {
            std::cerr << "Error: Could not reorder files.\n";
        }
    }
    else {
        std::cerr << "Error: Could not load disk.\n";
        diskname.clear();
    }
}

/// <summary>
/// Handle disk rename
/// </summary>
/// <param name="diskfile">diskfile to use</param>
/// <param name="newname">name name for disk</param>
void handleDiskRename(const std::string& diskfile, const std::string& newname)
{
    d64 disk;
    diskname = diskfile;

    if (disk.load(diskname)) {
        if (disk.rename_disk(newname)) {
            disk.save(diskfile);
            std::cout << "Renamed disk " << disk.diskname() << "\n";
        }
        else {
            std::cerr << "Error: Could not rename disk.\n";
        }
    }
    else {
        std::cerr << "Error: Could not load disk.\n";
        diskname.clear();
    }
}

/// <summary>
/// Load 2 disks in memory
/// and determine if they loaded correctly
/// </summary>
/// <param name="sourceDisk">gets the source disk</param>
/// <param name="targetDisk">gets the target disk</param>
/// <param name="source">source disk name</param>
/// <param name="target">target disk name</param>
/// <returns>true on success</returns>
bool loadDisks(d64& sourceDisk, d64& targetDisk, const std::string& source, const std::string& target)
{
    auto source_valid = sourceDisk.load(source);
    auto dest_valid = targetDisk.load(target);

    if (!source_valid || !dest_valid) {
        std::cerr << "Error: Failed to load one or both disks.\n";
        return false;
    }
    return true;
}

/// <summary>
/// return true if a file exists on a disk
/// </summary>
/// <param name="disk">disk to search</param>
/// <param name="filename">file to search for</param>
/// <returns>true on success</returns>
bool fileExists(d64& disk, const std::string& filename)
{
    return disk.findFile(filename).has_value();
}

/// <summary>
/// copy files from sourceDisk to targetDisk
/// if files wont fit create another target disk
/// </summary>
/// <param name="sourceDisk">source disk to copy</param>
/// <param name="targetDisk">gets the copied files</param>
/// <returns>true on success</returns>
bool copyFiles(d64& sourceDisk, d64& targetDisk)
{
    for (const auto& fileEntry : sourceDisk.directory()) {
        std::string filename = d64::Trim(fileEntry.file_name);

        if (fileExists(targetDisk, filename)) {
            auto valid = false;
            do {
                if (conformation != skip_all && conformation != overwrite_all) {
                    std::cout << "File \"" << filename << "\" already exists. Overwrite?  (y/n or a=all/x=none):";
                    char response;
                    std::cin >> response;
                    response = toupper(response);
                    switch (response) {
                        case 'Y':
                            conformation = overwrite_file;
                            valid = true;
                            break;
                        case 'N':
                            conformation = skip_file;
                            valid = true;
                            break;
                        case 'A':
                            conformation = overwrite_all;
                            valid = true;
                            break;
                        case 'X':
                            conformation = skip_all;
                            valid = true;
                            break;
                        default:
                            break;
                    }
                }
                else {
                    valid = true;
                }
            } while (!valid);

            if (conformation == skip_all || conformation == skip_file) {
                std::cout << "Skipping \"" << filename << "\"\n";
                continue;
            }
            std::cout << "overwriting \"" << filename << "\"\n";
            targetDisk.removeFile(filename);
        }

        // allow dest to have 2 free sectors
        if ((targetDisk.getFreeSectorCount() + 2) * 256 <= fileEntry.file_size[0] + fileEntry.file_size[1] * 256) {
            // This wont fit.
            backup_disk_num++;

            std::string target_name = target_backup_base_name + backup_disk_num + ".d64";
            targetDisk.formatDisk("BACKUP" + backup_disk_num);
            targetDisk.save(target_name);
        }

        auto fileData = sourceDisk.readFile(filename);
        if (fileData.has_value()) {
            targetDisk.addFile(filename, static_cast<FileTypes>(fileEntry.file_type), fileData.value());
        }
        else {
            std::cerr << "Error: Failed to copy \"" << filename << "\"\n";
            return false;
        }

    }
    return true;
}

/// <summary>
/// Backup up source to target
/// </summary>
/// <param name="source">source disk name</param>
/// <param name="target">target disk name</param>
void Backup(const std::string& source, const std::string& target)
{
    d64 sourceDisk, targetDisk;

    if (!loadDisks(sourceDisk, targetDisk, source, target)) {
        return;
    }

    if (!copyFiles(sourceDisk, targetDisk)) {
        std::cerr << "Error: Backup failed.\n";
        return;
    }

    targetDisk.save(target);
}

/// <summary>
/// backup disks
/// </summary>
/// <param name="diskfile">diskfile to use</param>
/// <param name="order">disks to backup files. If there are files not on the list they are put at the end</param>
void handleBackup(const std::string& diskfile, const std::vector<std::string>& disks)
{
    d64 target;
    d64 source;
    target_backup_base_name = diskfile;
    if (diskfile.ends_with(".d64") || diskfile.ends_with(".D64")) {
        target_backup_base_name = diskfile.substr(0, diskfile.length() - 4);
    }
    std::string target_name = target_backup_base_name + ".d64";
    if (!target.load(target_name)) {
        target.formatDisk("NEW DISK");
    }
    target.rename_disk("BACKUP");
    target.save(target_name);
    backup_disk_num = '0';
    conformation = skip_file;
    auto n = 0;
    for (auto& src : disks) {
        std::cout << "disk " << ++n << " of " << disks.size() << " " << src << '\n';
        Backup(src, target_name);
    }
    std::cout << "Backup complete: " << target_backup_base_name << ".d64" << "\n";
}

/// <summary>
/// Execute a interactive command
/// </summary>
/// <param name="command">command to ececute</param>
/// <param name="params">parameters for function</param>
void executeCommand(const std::string& command, std::vector<std::string>& params)
{
    auto flag = false;
    auto param_error = false;

    // if the user did not supply a diskname, use the last one
    if (!(params.size() > 0 && params[0].ends_with(".d64"))) {
        params.insert(params.begin(), diskname);
    }

    auto it = functionTable.find(command);
    if (it == functionTable.end()) {
        std::cerr << "Error: Unknown command \"" << command << "\"\n";
        return;
    }

    const auto& entry = functionTable[command];
    switch (entry.type) {
        case no_param:
            entry.f0();
            break;

        case one_param:
            if (!(param_error = (params.size() < 1))) entry.f1(params[0]);
            break;

        case two_param:
            if (!(param_error = (params.size() < 2))) entry.f2(params[0], params[1]);
            break;

        case three_param:
            if (!(param_error = (params.size() < 3))) entry.f3(params[0], params[1], params[2]);
            break;

        case two_bool:
            flag = false;
            if (params.size() > 1) {
                flag = (params[1] == "true");
            }
            entry.fb(params[0], flag);
            break;

        case file_list:
            if (!(param_error = (params.size() < 1))) {
                std::vector<std::string> fileList(params.begin() + 1, params.end());
                entry.fn(params[0], fileList);
            }
            break;

        case two_int:
            if (!(param_error = (params.size() < 3))) {
                std::string s = params[1];
                int track = std::stoi(s);
                s = params[2];
                int sector = std::stoi(s);
                entry.fi(params[0], track, sector);
            }
            break;

        default:
            std::cerr << "Error: Unknown command type\n";
    }
    if (param_error) {
        std::cerr << "Error: Missing parameters for command " << command << "\n";
    }
    return;
}

// <summary>
/// Interactive shell to execute commands
/// </summary>
void interactiveShell()
{
    std::string input;
    std::cout << "d64 CLI Interactive Mode (type 'exit' to quit type load <diskname> to load a disk)\n";

    while (true) {
        std::cout << '[' << (diskname.size() > 0 ? diskname : "no disk") << "] d64> ";
        std::getline(std::cin, input);
        if (input == "exit") break;

        std::istringstream iss(input);
        std::vector<std::string> args{ std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{} };

        if (args.empty()) continue;

        std::string command = args[0];
        args.erase(args.begin());
        try {
            executeCommand(command, args);
        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return;
        }
    }
}

/// <summary>
/// Main entry point
/// </summary>
/// <param name="argc"></param>
/// <param name="argv"></param>
/// <returns></returns>
int main(int argc, char* argv[])
{
    program.add_argument("command")
        .help("Command to execute (create, format, add, addrel, list, dir, extract, remove, rename, verify, compact, bam, dump, lock, unlock, reorder, rename-disk)")
        .nargs(argparse::nargs_pattern::optional);

    program.add_argument("diskfile")
        .help("D64 disk image file")
        .nargs(argparse::nargs_pattern::optional);

    program.add_argument("filename")
        .help("File to add, extract, remove, lock or unlock")
        .nargs(argparse::nargs_pattern::optional);

    program.add_argument("newname")
        .help("New name for renaming a file or disk")
        .nargs(argparse::nargs_pattern::optional);

    program.add_argument("--fix")
        .help("Automatically fix BAM errors")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("--order")
        .help("List of filenames for reordering")
        .nargs(argparse::nargs_pattern::any);

    program.add_argument("--disks")
        .help("List of disks to backup")
        .nargs(argparse::nargs_pattern::any);

    program.add_argument("--tracks")
        .help("number of tracks to format (35 or 40)")
        .nargs(argparse::nargs_pattern::optional);

    program.add_argument("--recordsize")
        .help("record size for .rel files (2 - 254)")
        .nargs(argparse::nargs_pattern::optional);

    program.add_argument("--track")
        .help("Track to dump")
        .nargs(argparse::nargs_pattern::optional);

    program.add_argument("--sector")
        .help("Sector to dump")
        .nargs(argparse::nargs_pattern::optional);

    program.add_argument("--interactive")
        .help("Launch interactive shell mode")
        .default_value(false)
        .implicit_value(true);

    try {
        if (argc == 1) {
            program.parse_args({argv[0], "--help"}); // Forces --help to be the default
        }
        else {
            program.parse_args(argc, argv);
        }
        if (program.get<bool>("--interactive")) {
            interactiveShell();
            return 0;
        }

        std::string command = program.get<std::string>("command");
        std::transform(command.begin(), command.end(), command.begin(), ::tolower);

        auto diskfile = program.get<std::string>("diskfile");
        if (command == "create" || command == "format") {
            auto use40tracks = false;
            try {
                auto tracks = program.get<std::string>("--tracks");
                if (tracks == "40" || tracks == "35") {
                    use40tracks = tracks == "40";
                }
                else {
                    std::cerr << "Invalid value for --tracks. Expecting 35 or 40.\n";
                    return 0;
                }
            }
            catch (const std::exception) {
                use40tracks = false;
            }
            handleCreate(diskfile, use40tracks);
        }
        else if (command == "add") {
            handleAdd(diskfile, program.get<std::string>("filename"));
        }
        else if (command == "addrel") {
            int recordsize = 0;
            auto rec = program.get<std::string>("--recordsize");
            recordsize = atoi(rec.c_str());
            handleAddRel(diskfile, program.get<std::string>("filename"), recordsize);
        }

        else if (command == "load") {
            handleLoad(diskfile);
        }
        else if (command == "bam") {
            handleBAM(diskfile);
        }
        else if (command == "list" || command == "dir") {
            handleList(diskfile);
        }
        else if (command == "extract") {
            handleExtract(diskfile, program.get<std::string>("filename"));
        }
        else if (command == "lock") {
            handleLock(diskfile, program.get<std::string>("filename"));
        }
        else if (command == "unlock") {
            handleUnlock(diskfile, program.get<std::string>("filename"));
        }
        else if (command == "remove") {
            handleRemove(diskfile, program.get<std::string>("filename"));
        }
        else if (command == "rename") {
            handleRename(diskfile, program.get<std::string>("filename"), program.get<std::string>("newname"));
        }
        else if (command == "verify") {
            handleVerify(diskfile, program.get<bool>("--fix"));
        }
        else if (command == "compact") {
            handleCompact(diskfile);
        }
        else if (command == "reorder") {
            handleReorder(diskfile, program.get<std::vector<std::string>>("--order"));
        }
        else if (command == "backup") {
            handleBackup(diskfile, program.get<std::vector<std::string>>("--disks"));
        }
        else if (command == "rename-disk") {
            handleDiskRename(diskfile, program.get<std::string>("filename"));
        }
        else if (command == "dump") {
            auto t = std::atoi(program.get<std::string>("--track").c_str());
            auto s = std::atoi(program.get<std::string>("--sector").c_str());
            handleDumpSector(diskfile, t, s);
        }
        else {
            std::cerr << "Unknown command.\n";
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
