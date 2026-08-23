#ifndef COMPILER_PIPELINE_HPP
#define COMPILER_PIPELINE_HPP

#include "Ast.hpp"
#include "FileReader/FileReader.hpp"
#include "Lexer/Lexer.hpp"
#include "Parser/Parser.hpp"
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>

// Represents a fully parsed file module
struct Module {
    std::string filePath;
    ExprPtr astRoot;
};

class CompilerPipeline {
    private:
        std::mutex queueMutex;
        std::queue<std::string> workQueue;
        std::unordered_set<std::string> visitedFiles;

        std::mutex astMutex;
        std::unordered_map<std::string, Module> parsedModules;

    public:
        int nbtokens = 0;
        void enqueueFile(const std::string& path) {
            std::lock_guard<std::mutex> lock(this->queueMutex);
            if (this->visitedFiles.find(path) == this->visitedFiles.end()) {
                this->visitedFiles.insert(path);
                this->workQueue.push(path);
            }
            else {
                std::cerr << "\033[31m\033[1m[Import Error]\033[0m\033[31m Tried to import already imported file '" << path << "': cannot perform circular import.\033[0m\n";
            }
        }

        // Process everything in the queue using hardware worker threads
        void runPipeline() {
            unsigned int threadCount = std::thread::hardware_concurrency();
            if (threadCount == 0) threadCount = 2; // Fallback

            std::vector<std::thread> workers;
            for (unsigned int i = 0; i < threadCount; ++i) {
                workers.emplace_back(&CompilerPipeline::workerLoop, this);
            }

            // Wait for all worker threads to finish draining the queue
            for (auto& t : workers) {
                if (t.joinable()) t.join();
            }
        }

    private:
        void workerLoop() {
            while (true) {
                std::string fileToParse;

                {
                    std::lock_guard<std::mutex> lock(this->queueMutex);
                    if (this->workQueue.empty()) return;
                    fileToParse = this->workQueue.front();
                    this->workQueue.pop();
                }
                Module module = this->parseSingleFile(fileToParse);

                {
                    std::lock_guard<std::mutex> lock(this->astMutex);
                    this->parsedModules[fileToParse] = std::move(module);
                }
            }
        }

        Module parseSingleFile(const std::string& path) {
            // Read file from disk
            std::optional<std::string> content = readFile(path.c_str());
            if (!content.has_value())
            {
                std::cerr << "File : " << path << " was not found\n";

                return Module{ path, nullptr }; 
            }

            const std::string &code = content.value();

            //std::cout << "File parsed : " << path << "\n";
            // Running the Lexer 
            Lexer l(code);
            std::vector<Token> codeTokenized = l.Tokenize();
            this->nbtokens += codeTokenized.size(); 
            //for (Token t : codeTokenized)
                //std::cout << t.toString();

            // Run Parser -> ExprPtr ast
            Parser p(std::move(codeTokenized), path, this);
            ExprPtr file = p.parseModule();
            // Whenever parser finds an `ImportExpr`, call: enqueueFile(importedPath);
            return Module{ path, std::move(file)}; 
        }
};

#endif // COMPILER_PIPELINE_HPP
