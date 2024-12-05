/**************************************************************************************
* cacheSimu.cpp
*
* Project Team: Le Quang Kiet, Vo Dai Thuan, Le Gia Vinh
* Class: 21ES
* Term: Simester 1 - 2024
*
* This file contains the source code for the simulation of a cache controller with a split
* 2-way set associative instruction cache and an 4-way set associative data cache.
*
***************************************************************************************/

#include <iostream>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string.h>
#include <math.h>

using namespace std;
unsigned int mode = 2;

/**************************************************************************************

*				CACHE SUBFUNCTIONS

**************************************************************************************/

// Cache Structure
//		Address can be left and right shifted corresponding to the number of bits to isolate that area
//		[               Address 32-bits                   ]
//		[ Tag 12-bits | Index 14-bits | Byte Offset 6-bits]

// Cache Line Structure
struct CacheLine {
    bool valid = false;
    bool dirty = false;
    unsigned int tag = -1;
    unsigned int offset = 0;
    int lru = 0; // LRU counter
};

// Cache Statistics
struct CacheStats {
    int reads = 0;
    int writes = 0;
    int hits = 0;
    int misses = 0;

    void display(const string &cacheName) const {
        cout << "Statistics for " << cacheName << ":\n";
        cout << "  Cache Reads: " << reads << "\n";
        cout << "  Cache Writes: " << writes << "\n";
        cout << "  Cache Hits: " << hits << "\n";
        cout << "  Cache Misses: " << misses << "\n";
        double hitRatio = (reads + writes) > 0 ? (double)hits / (reads + writes) : 0.0;
        cout << "  Cache Hit Ratio: " << fixed << setprecision(2) << hitRatio * 100 << "%\n";
    }
};

// Cache Class
class Cache {
private:
    int numSets;
    int associativity;
    int blockSize;
    bool isWriteThrough;
    vector<vector<CacheLine>> sets;
    CacheStats stats;
    int offsetBits; 

public:
    Cache(int sets, int assoc, int blockSize, bool writeThrough)
        : numSets(sets), associativity(assoc), blockSize(blockSize), isWriteThrough(writeThrough) {
        this->sets = vector<vector<CacheLine>>(numSets, vector<CacheLine>(associativity));
    }

    bool access(unsigned int address, bool isWrite, bool &writeBack, unsigned int &evictedAddress) {
        if (isWrite) stats.writes++;
        else stats.reads++;

        unsigned int setIndex = (address / blockSize) % numSets;
        unsigned int tag = address / (blockSize * numSets);
        unsigned int offset = address % blockSize;

        // Search for the tag in the set
        auto &set = sets[setIndex];
        for (int i = 0; i < associativity; ++i) {
            if (set[i].valid && set[i].tag == tag) {
                // Cache hit
                stats.hits++;
                if (isWrite && !isWriteThrough) set[i].dirty = true;
                updateLRU(set, i);
                return true;
            }
        }

        // Cache miss
        stats.misses++;
        
        // Eviction required
        int evictIndex = findLRUIndex(set);
        if (set[evictIndex].valid) {
            if (set[evictIndex].dirty && !isWriteThrough) {
                writeBack = true;
                evictedAddress = (set[evictIndex].tag * numSets + setIndex) * blockSize + offset;
            }
        }

        // Replace with new line
        set[evictIndex] = {true, isWrite && !isWriteThrough, tag, offset, 0};
        updateLRU(set, evictIndex);
        return false;
    }
	
	// Evict Function
	void evict(unsigned int address) {
    	unsigned int setIndex = (address / blockSize) % numSets;
    	unsigned int tag = address / (blockSize * numSets);

    	auto &set = sets[setIndex];
    	for (int i = 0; i < associativity; ++i) {
     	   if (set[i].valid && set[i].tag == tag) {
    	        cout << "Invalidate L1 line: Set " << setIndex << ", Way " << i << ", Tag 0x" << hex << set[i].tag << dec << endl;
    	        set[i].valid = false;
    	        set[i].dirty = false; // Reset dirty bit
    	        return;
    	    }
   		}
	}

	// Reset Cache
	void clearCache() {
        for (auto &set : sets) {
            for (auto &line : set) {
                line = {false, false, 0, 0};
            }
        }
        stats = {0, 0, 0, 0}; // Reset statistics
    }

    void printCacheState(ostream &out) {

    	out << "Cache State:\n";
    	out << "|Set  | Way | Valid | Dirty | Tag       | LRU | Index Bits | Offset Bits|\n";
    	out << "|-----|-----|-------|-------|-----------|-----|------------|------------|\n";

        for (int i = 0; i < numSets; ++i) {
            for (int j = 0; j < associativity; ++j) {
                const auto &line = sets[i][j];
                if (line.valid) {   
 				out  << "|" << setw(5) << i << "| "
                     	    << setw(3) << j << " | "
                    	    << setw(5) << line.valid << " | "	
                     		<< setw(5) << line.dirty << " | "
                     		<< setw(10) << hex << line.tag << dec << "| "
                     		<< setw(3) << line.lru << " | "
                     		<< setw(10) << hex << i << dec << " | "
                     		<< setw(11) << hex << line.offset << dec << "|\n"; 
                }
            }
        }

    out << "|-----|-----|-------|-------|-----------|-----|------------|------------|\n";
    }

    void displayStatistics(const string &cacheName) const {
        stats.display(cacheName);
    }

private:
	// LRU Replacement policy
    void updateLRU(vector<CacheLine> &set, int accessedIndex) {
        for (int i = 0; i < associativity; ++i) {
            if (i != accessedIndex && set[i].valid) set[i].lru++;
        }
        set[accessedIndex].lru = 0;
    }

    int findLRUIndex(const vector<CacheLine> &set) const {
        int maxLRU = -1, lruIndex = 0;
        for (int i = 0; i < associativity; ++i) {
            if (!set[i].valid) return i; // Prefer invalid entry
            if (set[i].lru > maxLRU) {
                maxLRU = set[i].lru;
                lruIndex = i;
            }
        }
        return lruIndex;
    }
};

// Simulation Functions
//void processTrace(const string &traceFile, Cache &instructionCache, Cache &dataCache) {
//    ifstream file(traceFile);
void processTrace(Cache &instructionCache, Cache &dataCache, const string &inputFile, const string &outputFile) {
    ifstream fileIn(inputFile);
    ofstream fileOut(outputFile);
    if (!fileIn.is_open()) {
        cerr << "Error: Unable to open " << inputFile << endl;
        return;
    }
    if (!fileOut.is_open()) {
        cerr << "Error: Unable to open " << outputFile << endl;
        return;
    }
	
	cout << "Processing trace file: " << inputFile << endl;
	
    string line;
    while (getline(fileIn, line)) {
        stringstream ss(line);
        int operation;
        unsigned int address;
        ss >> operation >> hex >> address;

        bool writeBack = false;
        unsigned int evictedAddress = 0;
		
		
		
        switch (operation) {
            case 0: // Read data
                if (!dataCache.access(address, false, writeBack, evictedAddress)) {
                	if(mode>0){
                    	cout << "DataCache: Read from L2 0x" << hex << address << dec << endl;
                    
                    	cout << " [Read-miss]\n";
                    	
//                    	cout << "Data ";
//                    	dataCache.printCacheState(cout);
               		
					}
                }else{
                	if(mode>0){
					cout << "DataCache: Read from L2 0x" << hex << address << dec << endl;
                    
                    cout << " [Read-hit]\n";
                    	
//                    cout << "Data ";
//                    dataCache.printCacheState(cout);
                	}
				}
                if (writeBack) {
                	if(mode>0){
                    cout << "DataCache: Write to L2 0x" << hex << evictedAddress << dec << endl;
//                    cout << "Data ";
//                    dataCache.printCacheState(cout);
                	}
                }
                break;
            case 1: // Write-back data
                if (!dataCache.access(address, true, writeBack, evictedAddress)) {
                	if(mode>0){
                    	cout << "DataCache: Read for Ownership from L2 [Write-back] 0x" << hex << address << dec << endl;
                    	
                    	cout << " [Write-miss]\n";
                    	
//                    	cout << "Data ";
//                    	dataCache.printCacheState(cout);
                	}
                }else{
                	if(mode>0){
                    	cout << "DataCache: Read for Ownership from L2 [Write-back] 0x" << hex << address << dec << endl;
                    	
                    	cout << " [Write-hit]\n";
                    	
//                    	cout << "Data ";
//                    	dataCache.printCacheState(cout);
                	}
				}
                if (writeBack) {
                	if(mode>0){
                    cout << "DataCache: Write to L2 0x" << hex << evictedAddress << dec << endl;
                    cout << " [Evict]\n";
//                    cout << "Data ";
//                    dataCache.printCacheState(cout);
                	}
                }
                break;
            case 2: // Instruction fetch
                if (!instructionCache.access(address, false, writeBack, evictedAddress)) {
                	if(mode>0){
                    	cout << "InstructionCache: Read from L2 0x" << hex << address << dec << endl;
                    	
                    	cout << " [Read-miss]\n";
                    	
//                    	cout << "Instruction ";
//                    	instructionCache.printCacheState(cout);
                	}
                }else {
                	if(mode>0){
                    	cout << "InstructionCache: Read from L2 0x" << hex << address << dec << endl;
                    	
                    	cout << " [Read-hit]\n";
                    	
//                    	cout << "Instruction ";
//                    	instructionCache.printCacheState(cout);
                	}
				}
                break;
            case 3: // Evict command from L2
            	if(mode>0){
                cout << "Evict from L2 0x" << hex << address << dec << endl;
                dataCache.evict(address); // Invalidate line in L1 Data Cache
                instructionCache.evict(address); // Invalidate line in L1 Instruction Cache
//                cout << "Data ";
//                dataCache.printCacheState(cout);
            	}
                break;
                
            case 8: // Clear cache
                instructionCache.clearCache();
                dataCache.clearCache();
                cout << "Cache cleared and statistics reset." << endl;
                cout << "------------------------------------------------------------------------\n";
                break;

            case 9: // Print cache state 			
            	cout << "\n";
            	cout << "------------------------------------------------------------------------\n";
            	cout << "----------------------Cache State After Simulation----------------------\n";
            	cout << "------------------------------------------------------------------------\n";
            	
            	fileOut << "\t\t\tInstruction Cache\n";
                cout << "\t\t\tInstruction Cache\n";
                instructionCache.printCacheState(fileOut);
                instructionCache.printCacheState(cout);
                instructionCache.displayStatistics("Instruction Cache");
                
                fileOut << "\t\t\tData Cache\n";
                cout << "\t\t\tData Cache\n";
                dataCache.printCacheState(fileOut);
                dataCache.printCacheState(cout);
                dataCache.displayStatistics("Data Cache");
                
				
                
			    cout << "\tCache state has been written to " << "CacheStateOutputFile.txt" << endl;
			    cout << "------------------------------------------------------------------------\n";
                break;

            default:
                cout << "Invalid operation: " << operation << endl;
                break;
        }
    }
	
    fileIn.close();
    fileOut.close();
}

/**************************************************************************************

*				MAIN CACHE CONTROLLER

**************************************************************************************/

int main(int argc, char *argv[]) {
	//If using command line
//	if(argc != 3){
//		printf("Mode and Trace File required \n Enter mode first and then file \n");
//		printf("Mode is 0 or 1, filename format is '<file>.txt'\n");
//		return -1;
//	}
//	else if(argc == 3){
//		mode = atoi(argv[1]);
//		trace_file = argv[2];
//	}
//	else{
//		printf("ERROR");
//		return -1;
//	}
//    	processTrace(instructionCache, dataCache, fileIn, fileOut);
//    	cout << "Processing completed. Results written to " << fileOut << endl;
//    	cout << "------------------------------------------------------------------------\n";
    // Set Cache parameters
    const int L1Sets = 16384; // 16K sets
    const int blockSize = 64; // 64 bytes per line

    Cache instructionCache(L1Sets, 2, blockSize, false); // 2-way set associative
    Cache dataCache(L1Sets, 4, blockSize, false);        // 4-way set associative

	// Initialize the cache at the beginning
	cout << "------------------------------------------------------------------------\n";
	cout << "----------------------------CACHE INITIALIZE----------------------------\n";
    instructionCache.clearCache();
    dataCache.clearCache();
	cout << "\t\t\tInstruction Cache\n";
    instructionCache.printCacheState(cout);
    instructionCache.displayStatistics("Instruction Cache");
    cout << "\t\t\tData Cache\n";
    dataCache.printCacheState(cout);
    dataCache.displayStatistics("Data Cache");
    
    const string fileOut = "CacheStateOutputFile.txt"; // Default output file
    

	
	// Select a mode and input, output file

	while(true){
		
		cout << "------------------------------------------------------------------------\n";
		cout << "\t Select Mode and File To Start Simulation" << endl;
		cout << "[Mode 0]: Summary of usage statistics and print commands only" << endl;
		cout << "[Mode 1]: Information from Mode 0 with messages to L2 in addition" << endl;
		cout << "[Note]: Input Trace File Name Syntax: <input_file_name.txt>" << endl;
		cout << "[Note]: The final result will be wrote to " << fileOut << endl;
		
		// Select file
        cout << "\nEnter the input trace file name (or type 'stop' to exit): ";
        string fileIn;
        getline(cin, fileIn);
		
		if (fileIn == "stop") {
            cout << "\n\t\tExiting simulation. Goodbye!" << endl;
            break;
        }
		
		// Select mode
		while (true) {
        	cout << "Enter Mode (0 or 1): ";
        	cin >> mode;

        	if (mode == 0 || mode == 1) break;

        	cout << "Invalid mode! Please enter 0 or 1.\n";
    	}
		cin.ignore();
		
		cout << "------------------------------------------------------------------------\n";
		cout << "----------------------------START SIMULATION----------------------------\n";
		cout << "------------------------------------------------------------------------\n";
		if(mode == 1){
			cout << "--------------------------L1/L2 Communication---------------------------\n";
		}
		
		    // Process trace file
    	processTrace(instructionCache, dataCache, fileIn, fileOut);
    	cout << "Processing completed. Results written to " << fileOut << endl;
    	cout << "------------------------------------------------------------------------\n";
		
	}



    
    //For cmd line
//    processTrace(argv[1], instructionCache, dataCache);

	cout <<"\n\n\t\tTesting Completed: Closing Program... \n\n\n";

    return 0;
}
