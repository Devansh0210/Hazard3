#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint>
#include <string>
#include <algorithm>
#include <stdio.h>

// Device-under-test model generated by CXXRTL:
#include "dut.cpp"
#include <backends/cxxrtl/cxxrtl_vcd.h>

static const unsigned int MEM_SIZE = 16 * 1024 * 1024;
uint8_t mem[MEM_SIZE];

static const unsigned int IO_BASE = 0x80000000;
enum {
	IO_PRINT_CHAR = 0,
	IO_PRINT_U32  = 4,
	IO_EXIT       = 8
};

const char *help_str =
"Usage: tb binfile cmdlist [vcdfile] [--dump start end] [--cycles n]\n"
"    binfile          : Binary to load into start of memory\n"
"    cmdlist          : Debug module command list file\n"
"    vcdfile          : Path to dump waveforms to\n"
"    --dump start end : Print out memory contents between start and end (exclusive)\n"
"                       after execution finishes. Can be passed multiple times.\n"
"    --cycles n       : Maximum number of cycles to run before exiting.\n"
;

void exit_help(std::string errtext = "") {
	std::cerr << errtext << help_str;
	exit(-1);
}

enum cmdstate {
	S_IDLE = 0,
	S_WRITE_SETUP,
	S_WRITE_ACCESS,
	S_READ_SETUP,
	S_READ_ACCESS
};

int main(int argc, char **argv) {

	if (argc < 3)
		exit_help();

	bool dump_waves = false;
	std::string waves_path;
	std::vector<std::pair<uint32_t, uint32_t>> dump_ranges;
	int64_t max_cycles = 100000;

	for (int i = 3; i < argc; ++i) {
		std::string s(argv[i]);
		if (i == 3 && s.rfind("--", 0) != 0) {
			// Optional positional argument: vcdfile
			dump_waves = true;
			waves_path = s;
		}
		else if (s == "--dump") {
			if (argc - i < 3)
				exit_help("Option --dump requires 2 arguments\n");
			dump_ranges.push_back(std::pair<uint32_t, uint32_t>(
				std::stoul(argv[i + 1], 0, 0),
				std::stoul(argv[i + 2], 0, 0)
			));;
			i += 2;
		}
		else if (s == "--cycles") {
			if (argc - i < 2)
				exit_help("Option --cycles requires an argument\n");
			max_cycles = std::stol(argv[i + 1], 0, 0);
			i += 1;
		}
		else {
			std::cerr << "Unrecognised argument " << s << "\n";
			exit_help("");
		}
	}

	cxxrtl_design::p_tb top;

	std::fill(std::begin(mem), std::end(mem), 0);

	std::ifstream fd(argv[1], std::ios::binary | std::ios::ate);
	std::streamsize bin_size = fd.tellg();
	if (bin_size > MEM_SIZE) {
		std::cerr << "Binary file (" << bin_size << " bytes) is larger than memory (" << MEM_SIZE << " bytes)\n";
		return -1;
	}
	fd.seekg(0, std::ios::beg);
	fd.read((char*)mem, bin_size);

	std::ifstream cmdfile(argv[2]);

	std::ofstream waves_fd;
	cxxrtl::vcd_writer vcd;
	if (dump_waves) {
		waves_fd.open(waves_path);
		cxxrtl::debug_items all_debug_items;
		top.debug_info(all_debug_items);
		vcd.timescale(1, "us");
		vcd.add(all_debug_items);
	}

	bool bus_trans = false;
	bool bus_write = false;
	bool bus_trans_i = false;
	uint32_t bus_addr_i = 0;
	uint32_t bus_addr = 0;
	uint8_t bus_size = 0;
	// Never generate bus stalls
	top.p_i__hready.set<bool>(true);
	top.p_d__hready.set<bool>(true);

	// Reset + initial clock pulse
	top.step();
	top.p_clk.set<bool>(true);
	top.step();
	top.p_clk.set<bool>(false);
	top.p_rst__n.set<bool>(true);
	top.step();

	cmdstate state = S_IDLE;
	int idle_counter = 0;

	for (int64_t cycle = 0; cycle < max_cycles; ++cycle) {
		top.p_clk.set<bool>(false);
		top.step();
		if (dump_waves)
			vcd.sample(cycle * 2);
		top.p_clk.set<bool>(true);
		top.step();

		bool got_exit_cmd = false;

		switch (state) {
		case S_IDLE:
			if (idle_counter > 0) {
				--idle_counter;
			}
			else {
				std::string line;
				do {
					if (!std::getline(cmdfile, line))
						line = "i 1000";
				} while (line.length() == 0 || line[0] == '#');

				std::istringstream iss(line);
				iss >> std::setbase(0);
				std::string verb;
				iss >> verb;
				if (verb == "i") {
					iss >> idle_counter;
					printf("i %d\n", idle_counter);
				}
				else if (verb == "w") {
					uint32_t addr, data;
					iss >> addr;
					iss >> data;
					top.p_dmi__paddr.set<uint32_t>(addr);
					top.p_dmi__pwdata.set<uint32_t>(data);
					top.p_dmi__psel.set<bool>(true);
					top.p_dmi__pwrite.set<bool>(true);
					state = S_WRITE_SETUP;
					printf("w %02x: %08x\n", addr, data);
				}
				else if (verb == "r") {
					uint32_t addr;
					iss >> addr;
					top.p_dmi__paddr.set<uint32_t>(addr);
					top.p_dmi__psel.set<bool>(true);
					top.p_dmi__pwrite.set<bool>(false);
					state = S_READ_SETUP;
				}
				else if (verb == "x") {
					got_exit_cmd = true;
				}
				else {
					std::cerr << "Unrecognised verb " << verb << "\n";
					got_exit_cmd = true;
				}
			}
			break;
		case S_READ_SETUP:
			top.p_dmi__penable.set<bool>(true);
			state = S_READ_ACCESS;
			break;
		case S_READ_ACCESS:
			top.p_dmi__penable.set<bool>(false);
			top.p_dmi__psel.set<bool>(false);
			printf("r %02x: %08x\n", top.p_dmi__paddr.get<uint32_t>(), top.p_dmi__prdata.get<uint32_t>());
			state = S_IDLE;
			idle_counter = 10;
			break;
		case S_WRITE_SETUP:
			top.p_dmi__penable.set<bool>(true);
			state = S_WRITE_ACCESS;
			break;
		case S_WRITE_ACCESS:
			top.p_dmi__penable.set<bool>(false);
			top.p_dmi__psel.set<bool>(false);
			top.p_dmi__pwrite.set<bool>(false);
			state = S_IDLE;
			idle_counter = 10;
			break;
		default:
			state = S_IDLE;
			break;
		}

		// Handle current data phase, then move current address phase to data phase
		uint32_t rdata = 0;
		if (bus_trans && bus_write) {
			uint32_t wdata = top.p_d__hwdata.get<uint32_t>();
			if (bus_addr <= MEM_SIZE) {
				unsigned int n_bytes = 1u << bus_size;
				// Note we are relying on hazard3's byte lane replication
				for (unsigned int i = 0; i < n_bytes; ++i) {
					mem[bus_addr + i] = wdata >> (8 * i) & 0xffu;
				}
			}
			else if (bus_addr == IO_BASE + IO_PRINT_CHAR) {
				putchar(wdata);
			}
			else if (bus_addr == IO_BASE + IO_PRINT_U32) {
				printf("%08x\n", wdata);
			}
			else if (bus_addr == IO_BASE + IO_EXIT) {
				printf("CPU requested halt. Exit code %d\n", wdata);
				printf("Ran for %ld cycles\n", cycle + 1);
				break;
			}
		}
		else if (bus_trans && !bus_write) {
			if (bus_addr <= MEM_SIZE) {
				bus_addr &= ~0x3u;
				rdata =
					(uint32_t)mem[bus_addr] |
					mem[bus_addr + 1] << 8 |
					mem[bus_addr + 2] << 16 |
					mem[bus_addr + 3] << 24;
			}
		}
		top.p_d__hrdata.set<uint32_t>(rdata);
		if (bus_trans_i) {
			bus_addr_i &= ~0x3u;
			top.p_i__hrdata.set<uint32_t>(
				(uint32_t)mem[bus_addr_i] |
				mem[bus_addr_i + 1] << 8 |
				mem[bus_addr_i + 2] << 16 |
				mem[bus_addr_i + 3] << 24
			);
		}

		bus_trans = top.p_d__htrans.get<uint8_t>() >> 1;
		bus_write = top.p_d__hwrite.get<bool>();
		bus_size = top.p_d__hsize.get<uint8_t>();
		bus_addr = top.p_d__haddr.get<uint32_t>();
		bus_trans_i = top.p_i__htrans.get<uint8_t>() >> 1;
		bus_addr_i = top.p_i__haddr.get<uint32_t>();

		if (dump_waves) {
			// The extra step() is just here to get the bus responses to line up nicely
			// in the VCD (hopefully is a quick update)
			top.step();
			vcd.sample(cycle * 2 + 1);
			waves_fd << vcd.buffer;
			vcd.buffer.clear();
		}
		if (got_exit_cmd)
			break;
	}

	for (auto r : dump_ranges) {
		printf("Dumping memory from %08x to %08x:\n", r.first, r.second);
		for (int i = 0; i < r.second - r.first; ++i)
			printf("%02x%c", mem[r.first + i], i % 16 == 15 ? '\n' : ' ');
		printf("\n");
	}

	return 0;
}
