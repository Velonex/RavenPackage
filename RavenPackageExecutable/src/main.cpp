#include <RavenPackage/RavenPackage.h>

int main(int argc, char** argv) {
	if (argc > 5) {
		std::cout << "Usage: ravenpackageexecutable [mode:-archive/-extract/-extractto] [dir/archive/archive] [archive/file path/file path] [-/-/output]" << std::endl;
		exit(64);
	}
	else if (argc == 4) {
		if (!strcmp(argv[1], "-archive")) {
			rvn::package::createArchiveFromDir(argv[2], argv[3]);
		}
		else if (!strcmp(argv[1], "-extract")) {
			rvn::package::extractFile(argv[2], argv[3]);
		}
		else {
			std::cout << "Invalid mode. Use -archive." << std::endl;
		}
	}
	else if (argc == 5) {
		if (!strcmp(argv[1], "-extractto")) {
			rvn::package::extractFile(argv[2], argv[3], argv[4]);
		}
		else {
			std::cout << "Invalid mode. Use -archive/-extract/-extractto." << std::endl;
		}
	}
	else {
		std::string path1;
		std::string path2;
		std::string path3;
		std::cout << "RavenPackage console interface: " << std::endl;
		std::cout << "Type ?exit to exit." << std::endl;
		while (true) {
			std::string choice;
			std::cout << "Archive (archive/extract/extractto/browse) > ";
			std::getline(std::cin, choice);
			if (choice == "extract") {
				std::cout << "Enter input path > ";
				std::getline(std::cin, path1);
				if (path1 == "?exit") exit(0);
				std::cout << "Enter file path > ";
				std::getline(std::cin, path2);
				if (path2 == "?exit") exit(0);
				rvn::package::extractFile(path1, path2);
			}
			else if (choice == "extractto") {
				std::cout << "Enter input path > ";
				std::getline(std::cin, path1);
				if (path1 == "?exit") exit(0);
				std::cout << "Enter file path > ";
				std::getline(std::cin, path2);
				if (path2 == "?exit") exit(0);
				std::cout << "Enter target path > ";
				std::getline(std::cin, path3);
				if (path2 == "?exit") exit(0);
				rvn::package::extractFile(path1, path2, path3);
			}
			else if (choice == "archive") {
				std::cout << "Enter directory path > ";
				std::getline(std::cin, path1);
				if (path1 == "?exit") exit(0);
				std::cout << "Enter archive path > ";
				std::getline(std::cin, path2);
				if (path2 == "?exit") exit(0);
				rvn::package::createArchiveFromDir(path1, path2, true);
			}
			else if (choice == "browse") {
				std::cout << "Enter archive path > ";
				std::string archivePath;
				std::getline(std::cin, archivePath);
				std::string path = "";
				for (;;) {
					auto entries = rvn::package::getEntriesAt(archivePath, path);
					if (entries.status != RPK_OK) {
						std::cout << "Error code: " << entries.status << std::endl;
						break;
					}
					std::cout << std::endl << "Files in archive " << archivePath << path << std::endl;
					for (auto& entry : entries.entries) {
						if (entry.isFile) {
							std::cout << "FILE";
							std::cout << "\t" << entry.formattedLength << "\t";
							std::cout << entry.name << std::endl;
						}
						else {
							std::cout << "DIR ";
							std::cout << "\t" << entry.name << std::endl;
							if (entry.hasSubFiles) {
								std::cout << "    \t" << (char)192 << "..." << std::endl;
							}
						}
					}
					bool exit = false;
					for (;;) {
						std::cout << std::endl << "Change to dir (?exit to exit)> ";
						std::string in;
						std::getline(std::cin, in);
						bool exists = false;
						for (auto& entry : entries.entries) {
							if (!entry.isFile) {
								if (entry.name == in) exists = true;
							}
						}
						if (exists) {
							path = path + "/" + in;
							exists = true;
						}
						else if (in == "..") {
							if (path.find('/') != path.npos) {
								path = path.substr(path.find_last_of('/'), 0);
								exists = true;
							}
						}
						else if (in == "?exit") {
							exit = true;
							break;
						}
						else {
							std::cout << "Invalid dir" << std::endl;
						}
						std::cout << std::endl;
						if (exists) break;
					}
					if (exit) break;
				}
			}
			else if (choice == "?exit") exit(0);
			else std::cout << "Invalid choice" << std::endl;
		}
	}
	return 0;
}