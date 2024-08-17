#include "json/json.h"
#include <iostream>
#include <fstream>

int main(int argc, char** argv){
	if(argc<2){
		std::cerr << "./refill <num_cores> [<filename>] [<dirname>]\n"; 
		return 1;
	}
	std::string filename = "IR.json";
	if(argc > 2){
		filename = std::string(argv[2]);
	}
	std::string dirname = "";
	if(argc > 3){
		dirname = std::string(argv[3]) + "/";
	}
	Json::Value IR;
	std::ifstream IRfile(filename);
	Json::Reader reader;
	reader.parse(IRfile, IR);
	IRfile.close();
	int num_cores = atoi(argv[1]);
	for(int i=0;i<num_cores;++i){
		if(IR.isMember(std::to_string(i))){
			Json::Value addr;
			std::ifstream addrfile(dirname + "wlist/"+std::to_string(i)+"/addr.json");
			reader.parse(addrfile, addr);
			addrfile.close();
			for(Json::Value &wl: IR[std::to_string(i)]){
				for(Json::Value &buffer: wl["buffer"]){
					buffer["address"] = addr[buffer["no"].asUInt()];
					buffer.removeMember("no");
				}
			}
		}
	}
	Json::StyledWriter writer;
	std::ofstream output(filename);
	output << writer.write(IR);
	return 0;
}