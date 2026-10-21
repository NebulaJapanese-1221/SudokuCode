#include "sudokucode.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace sc;

static std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) throw std::runtime_error("cannot open input: " + path);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(ifs)),
                                std::istreambuf_iterator<char>());
}

static void writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) throw std::runtime_error("cannot open output: " + path);
    ofs.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
}

static void printUsage(const char* prog) {
    std::cout << "SudokuCode - hexadecimal sudoku 2D barcode (Linux)\n\n"
              << "Usage: " << prog << " <mode> [options]\n\n"
              << "Modes:\n"
              << "  encode   Encode data into a SudokuCode\n"
              << "  decode   Decode a SudokuCode back to data\n\n"
              << "Options (encode):\n"
              << "  --input <file>        Input data file (required)\n"
              << "  --output <file>       Output .sudokucode file (default: output.sudokucode)\n"
              << "  --grid-size <4|9|16>  Grid dimension (default: 9)\n"
              << "  --ecc <0|1|2|3>       Reed-Solomon redundancy level (default: 2)\n"
              << "  --encrypt             Enable XOR encryption\n"
              << "  --key <string>        XOR key (required if --encrypt)\n"
              << "  --compress            Reserved (future zlib)\n"
              << "  --image <file>        Also render BMP image of the grid\n"
              << "  --cell-size <px>      BMP cell size in pixels (default: 20)\n"
              << "  --ascii               Print ASCII grid preview\n\n"
              << "Options (decode):\n"
              << "  --input <file>        Input .sudokucode or .bmp file (required)\n"
              << "  --output <file>       Output decoded data file\n"
              << "  --grid-size <4|9|16>  Grid dimension (default: 9)\n"
              << "  --ecc <0|1|2|3>       Must match encode level (default: 2)\n"
              << "  --encrypt             Must match encode setting\n"
              << "  --key <string>        XOR key (required if --encrypt)\n"
              << "  --image               Treat input as a BMP image\n"
              << "  --cell-size <px>      BMP cell size (default: 20)\n"
              << "  --ascii               Print ASCII grid preview\n\n"
              << "Examples:\n"
              << "  " << prog << " encode --input msg.txt --output code.sudokucode --ecc 2\n"
              << "  " << prog << " decode --input code.sudokucode --output recovered.bin\n"
              << "  " << prog << " encode --input msg.txt --output code.sudokucode --encrypt --key secret\n"
              << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    std::string mode = argv[1];

    std::string input, output, key;
    int gridSize = 9;
    int eccLevel = 2;
    bool encrypt = false;
    bool compress = false;
    bool ascii = false;
    std::string imageFile;
    int cellSize = 20;
    bool isImage = false;

    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--input" && i + 1 < argc) input = argv[++i];
        else if (a == "--output" && i + 1 < argc) output = argv[++i];
        else if (a == "--grid-size" && i + 1 < argc) gridSize = std::stoi(argv[++i]);
        else if (a == "--ecc" && i + 1 < argc) eccLevel = std::stoi(argv[++i]);
        else if (a == "--encrypt") encrypt = true;
        else if (a == "--key" && i + 1 < argc) key = argv[++i];
        else if (a == "--compress") compress = true;
        else if (a == "--ascii") ascii = true;
        else if (a == "--image" && i + 1 < argc) { imageFile = argv[++i]; isImage = true; }
        else if (a == "--image") isImage = true;
        else if (a == "--cell-size" && i + 1 < argc) cellSize = std::stoi(argv[++i]);
        else if (a == "--help" || a == "-h") { printUsage(argv[0]); return 0; }
        else { std::cerr << "Unknown option: " << a << "\n"; printUsage(argv[0]); return 1; }
    }

    Params p;
    p.n = gridSize;
    p.eccLevel = eccLevel;
    p.encrypt = encrypt;
    p.key = key;
    p.compress = compress;

    try {
        if (mode == "encode") {
            if (input.empty()) throw std::runtime_error("--input required for encode");
            if (output.empty()) output = "output.sudokucode";
            auto data = readFile(input);
            Code code;
            encode(data, p, code);
            if (!writeCodeFile(output, code))
                throw std::runtime_error("failed to write " + output);
            std::cout << "Encoded " << data.size() << " bytes -> " << output
                      << " (grid " << gridSize << "x" << gridSize
                      << ", ecc=" << eccLevel << ")\n";
            if (ascii) std::cout << asciiPreview(code);
            if (!imageFile.empty())
                writeBMP(imageFile, code, cellSize);
        } else if (mode == "decode") {
            if (input.empty()) throw std::runtime_error("--input required for decode");
            Code code;
            if (isImage) {
                if (!readBMP(input, code, cellSize))
                    throw std::runtime_error("failed to read BMP " + input);
            } else {
                if (!readCodeFile(input, code))
                    throw std::runtime_error("failed to read " + input);
            }
            auto data = decode(code, p);
            if (!output.empty()) {
                writeFile(output, data);
                std::cout << "Decoded " << data.size() << " bytes -> " << output << "\n";
            } else {
                std::cout.write(reinterpret_cast<const char*>(data.data()),
                                static_cast<std::streamsize>(data.size()));
                std::cout << "\nDecoded " << data.size() << " bytes.\n";
            }
            if (ascii) std::cout << asciiPreview(code);
        } else {
            std::cerr << "Unknown mode: " << mode << "\n";
            printUsage(argv[0]);
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
