#include <iostream>
#include <fstream>
typedef unsigned char byte;

struct IInputStream {
    std::ifstream file;
    explicit IInputStream(const std::string& filename): file(filename, std::ios::binary){
    }
    // Возвращает false, если поток закончился
    bool Read(byte& value){
        value = file.get();
        return file.good();
    }
    ~IInputStream(){file.close();}
};

struct IOutputStream {
    std::ofstream file;
    explicit IOutputStream(const std::string& filename): file(filename, std::ios::binary){}
    void Write(const byte& value){
        file.write((char *)&value, 1);
    }
    ~IOutputStream(){file.close();}
};

// Метод архивирует данные из потока original
void Encode(IInputStream& original, IOutputStream& compressed);
// Метод восстанавливает оригинальные данные
void Decode(IInputStream& compressed, IOutputStream& original);
