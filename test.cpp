#include "./models/filename_vocab_hash.h"
#include <iostream>

int main(){
    Vocab::FilenameVocab vocab;
    std::cout<< vocab.getId("语文") << std::endl;
    std::cout<< vocab.getId("数学") << std::endl;
    std::cout<< vocab.getId("英语") << std::endl;
    std::cout<< vocab.getId("物理") << std::endl;
    std::cout<< vocab.getId("化学") << std::endl;
    std::cout<< vocab.getId("生物") << std::endl;
    std::cout<< vocab.getId("班会") << std::endl;
    std::cout<< vocab.getId("未知类别") << std::endl;
    std::cout<< "-----------------" << std::endl;
    std::cout<< vocab.getWord(2) << std::endl;
    std::cout<< vocab.getWord(3) << std::endl;
    std::cout<< vocab.getWord(4) << std::endl;
    std::cout<< vocab.getWord(5) << std::endl;
    std::cout<< vocab.getWord(6) << std::endl;
    std::cout<< vocab.getWord(7) << std::endl;
    std::cout<< vocab.getWord(1000) << std::endl;
    std::cout<< vocab.getWord(10000) << std::endl;
    std::cout<< vocab.getWord(19999) << std::endl;
    std::cout<< vocab.getWord(20001) << std::endl;
    return 0;
}