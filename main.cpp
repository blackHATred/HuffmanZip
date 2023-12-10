#include <iostream>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <set>
#include <map>
#include "Huffman.h"

struct Node {
    size_t data;
    bool leaf;
    unsigned int freq;
    Node *left;
    Node *right;
    Node(size_t d, unsigned int f, Node *l, Node *r, bool le) : data(d), freq(f), left(l), right(r), leaf(le) {}
    ~Node() {
        delete left;
        delete right;
    }
};

// вспомогательная структура для кодирования
struct CodeInfo {
    unsigned int bits; // сколько бит занимает при кодировании
    size_t data; // оригинальные данные
    size_t code; // закодированные данные
    CodeInfo(unsigned int b, size_t d, size_t c) : bits(b), data(d), code(c) {}
};

class HuffmanTree {
private:
    Node *root;
    unsigned char bits;
    std::unordered_map<size_t, unsigned int> table;
    size_t _chunk = 0;
    size_t _chunk_fullness = 0;

    size_t calcTotalBits(Node *node, unsigned int len) {
        // считаем, сколько бит выйдет после сжатия (без учёта словаря)
        if (node->leaf)
            return node->freq * len;
        else
            // рекурсию будем считать допустимой для словарей не слишком больших последовательностей
            return calcTotalBits(node->left, len + 1) + calcTotalBits(node->right, len + 1);
    }

    std::vector<CodeInfo> getBytesDict(Node *node, size_t code, unsigned int len) {
        // Превращаем дерево в таблицу Хаффмана и сохраняем как вектор байтов для дальнейшей записи в файл
        if (node->leaf)
            return std::vector<CodeInfo>{CodeInfo(len, node->data, code)};
        else {
            // рекурсию будем считать допустимой для словарей не слишком больших последовательностей
            std::vector<CodeInfo> left = getBytesDict(node->left, code << 1, len + 1);
            std::vector<CodeInfo> right = getBytesDict(node->right, (code << 1) + 1, len + 1);
            // объединяем два вектора
            left.insert(left.end(), right.begin(), right.end());
            return left;
        }
    }

public:
    explicit HuffmanTree(unsigned short b) : bits(b), table(), root(nullptr) {}
    ~HuffmanTree() { delete root; }
    unsigned short getBits() const { return bits; }
    void calcTable(byte new_byte) {
        // строим таблицу с частотным анализом каждой последовательности бит длиной bits
        for (int i = 7; i >= 0; i--){
            _chunk_fullness++;
            _chunk = (_chunk << 1) + ((new_byte >> i) & 1);
            if (_chunk_fullness == bits){
                if (table.find(_chunk) == table.end())
                    table[_chunk] = 1;
                else
                    table[_chunk]++;
                _chunk_fullness = 0;
                _chunk = 0;
            }
        }
    }

    void buildTree() {
        // сначала посмотрим, заполнена ли статистика до конца, и по необходимости заполним её
        if(_chunk_fullness != 0){
            _chunk = (_chunk << (bits-_chunk_fullness));
            if (table.find(_chunk) == table.end())
                table[_chunk] = 1;
            else
                table[_chunk]++;
        }

        // строим дерево Хаффмана на основе частотного анализа
        std::vector<Node *> nodes;
        nodes.reserve(table.size());
        for (auto pair: table)
            nodes.push_back(new Node(pair.first, pair.second, nullptr, nullptr, true));
        // строим до тех пор, пока не останется одно общее дерево
        while (nodes.size() > 1) {
            std::sort(nodes.begin(), nodes.end(), [](Node *a, Node *b) { return a->freq > b->freq; });
            // объединяем два соседних узла в один общий
            auto node = new Node(0,
                                 nodes[nodes.size() - 1]->freq + nodes[nodes.size() - 2]->freq,
                                 nodes[nodes.size() - 2],
                                 nodes[nodes.size() - 1],
                                 false);
            nodes.pop_back();
            nodes.pop_back();
            nodes.push_back(node);
        }
        root = nodes[0];
    }

    size_t calcTotalBits() {
        // считаем, сколько бит выйдет после сжатия (без учёта словаря)
        return calcTotalBits(root, 0);
    }

    size_t calcEndSize() {
        // считает, сколько байт выйдет сжатая инфа
        size_t tmp = calcTotalBits();
        return 1+8 + 8 + (8 + 8 + 4) * getBytesDict().size() + tmp / 8 + (tmp % 8 == 0 ? 0 : 1);
    }

    std::vector<CodeInfo> getBytesDict() {
        // Превращаем дерево в таблицу Хаффмана и сохраняем как вектор байтов для дальнейшей записи в файл
        return getBytesDict(root, 0, 0);
    }

    std::vector<byte> getEncodedData(std::vector<byte> &dataToEncode, size_t total_bytes) {
        // для удобства сделаем таблицу для соотношения
        std::unordered_map<size_t, CodeInfo> encodeTable;
        for (auto item: getBytesDict()){
            // делаем соотношение Оригинальный блок - закодированный блок (CodeInfo)
            encodeTable.insert({item.data, item});
        }
        // вектор байтов, которые должны отдать
        std::vector<byte> encoded;
        // мы должны зарезервировать 1 байт на основание декодирования, 8 байт на итоговое количество байт, 8 байт на
        // количество записей в словаре, словарь (4+8+8 байт одна запись), размер закодированного сообщения и +1 на
        // возможное дозаписывание нулей
        encoded.reserve(calcEndSize());
        // записываем основание декодирования
        encoded.push_back(bits);
        // записываем размер оригинала
        for (int i = 7; i >= 0; i--)
            encoded.push_back((total_bytes >> (i * 8)) & 0xFF);
        // записываем размер словаря
        for (int i = 7; i >= 0; i--)
            encoded.push_back((encodeTable.size() >> (i * 8)) & 0xFF);
        // записываем сам словарь
        for (auto item: encodeTable) {
            for (int i = 3; i >= 0; i--)
                encoded.push_back((item.second.bits >> (i * 8)) & 0xFF);
            for (int i = 7; i >= 0; i--)
                encoded.push_back((item.second.code >> (i * 8)) & 0xFF);
            for (int i = 7; i >= 0; i--)
                encoded.push_back((item.second.data >> (i * 8)) & 0xFF);
        }
        // побайтово записываем закодированное содержимое в оставшуюся часть вектора
        byte current; // текущий наполняемый блок
        unsigned char current_fullness = 0; // заполненность текущего наполняемого блока
        size_t coding_block = 0; // набор бит, который мы должны закодировать
        unsigned char counter = 0; // наполненность набора бит
        for (byte seek_byte : dataToEncode) {
            for (int seek_bit = 7; seek_bit >= 0; seek_bit--) {
                coding_block = (coding_block << 1) + ((seek_byte >> seek_bit) & 1);
                counter++;
                if (counter == bits) {
                    unsigned int tmp_bits = encodeTable.find(coding_block)->second.bits; // сколько закодированных бит
                    coding_block = encodeTable.find(coding_block)->second.code; // закодированный блок
                    for (int i = tmp_bits - 1; i >= 0; i--) {
                        current = (current << 1) + ((coding_block >> i) & 1);
                        current_fullness++;
                        // если потребовалось сбросить блок в выдачу
                        if (current_fullness == 8) {
                            encoded.push_back(current); // сбрасываем блок
                            current_fullness = 0; // обнуляем заполненность блока
                            current = 0; // обнуляем сам блок
                        }
                    }
                    counter = 0; // обнуляем счётчик и идём дальше
                    coding_block = 0;
                }
            }
        }
        // дописываем остаток при необходимости
        if (current_fullness != 0) {
            current = (current << (8 - current_fullness));
            encoded.push_back(current); // сбрасываем блок
        }
        return encoded;
    }
};

void Encode(IInputStream &original, IOutputStream &compressed) {
    std::vector<byte> data; // все считанные байты
    byte value; // текущий считанный байт
    size_t total_bytes = 0; // общее количество считанных байт
    // будем строить сразу несколько деревьев Хаффмана, затем выясним, какое эффективнее
    std::vector<HuffmanTree> huffs = {
            HuffmanTree(2),
            HuffmanTree(3),
            HuffmanTree(4),
            HuffmanTree(5),
            HuffmanTree(6),
            HuffmanTree(7),
            HuffmanTree(8),
    };
    while (original.Read(value)) {
        for (auto &huff: huffs)
            huff.calcTable(value);
        data.push_back(value);
        total_bytes++;
    }
    // размер считанного потока данных
    // std::cout << total_bytes << "B" << std::endl;
    // строим деревья
    for (auto &huff: huffs) {
        huff.buildTree();
        // std::cout << huff.getBits() <<  " " << huff.calcEndSize() << "B" << std::endl;
    }
    // по самому эффективному способу сжатия записываем сжатые данные во второй поток
    for (auto b: std::min_element(huffs.begin(), huffs.end(), [](HuffmanTree &a, HuffmanTree &b) {
        return a.calcEndSize() < b.calcEndSize();
    })->getEncodedData(data, total_bytes)) {
        compressed.Write(b);
    }
}

void Decode(IInputStream &compressed, IOutputStream &original) {
    byte value; // текущий считанный байт
    // считываем основание декодирования
    unsigned char decode_bits = 0;
    compressed.Read(value);
    decode_bits = value;
    // читаем, сколько байт должно выйти в итоге
    size_t total_bytes = 0;
    for (int i = 0; i < 8; i++){
        compressed.Read(value);
        total_bytes = (total_bytes << 8) + value;
    }
    //std::cout << total_bytes << std::endl;
    // считываем размер словаря
    size_t dictSize = 0;
    for (int i = 0; i < 8; i++){
        compressed.Read(value);
        dictSize = (dictSize << 8) + value;
    }
    // считываем сам словарь
    std::map<std::pair<unsigned int, size_t>, size_t> dict;
    // bits - code - data
    for (int i = 0; i < dictSize; i++){
        // считываем количество бит
        unsigned int bits = 0;
        for (int j = 0; j < 4; j++){
            compressed.Read(value);
            bits = (bits << 8) + value;
        }
        // считываем как кодируется
        size_t code = 0;
        for (int j = 0; j < 8; j++){
            compressed.Read(value);
            code = (code << 8) + value;
        }
        // считываем как декодируется
        size_t data = 0;
        for (int j = 0; j < 8; j++){
            compressed.Read(value);
            data = (data << 8) + value;
        }
        // сохраняем в словарь
        dict[{bits, code}] = data;
    }
    // декодируем оставшуюся часть и отправляем на выход
    size_t current_buffer = 0;
    unsigned char current_buffer_fullness = 0;
    byte current_output = 0;
    unsigned char current_output_fullness = 0;
    while (compressed.Read(value)){
        for (int i = 7; i >= 0; i--){
            current_buffer = (current_buffer << 1) + ((value >> i) & 1);
            current_buffer_fullness++;
            auto tmp = dict.find({current_buffer_fullness, current_buffer});
            if (tmp != dict.end()){
                // нашли совпадение по словарю, можем декодировать
                for (int j = decode_bits-1; j >= 0; j--){
                    current_output = (current_output << 1) + ((tmp->second >> j) & 1);
                    current_output_fullness++;
                    if (current_output_fullness == 8){
                        // скидываем блок в отдачу
                        original.Write(current_output);
                        current_output = 0;
                        current_output_fullness = 0;
                        total_bytes--;
                        if (total_bytes == 0)
                            return;
                    }
                }
                current_buffer = 0;
                current_buffer_fullness = 0;
            }
        }
    }
}


int main() {
    auto inputStream1 = new IInputStream("input.txt");
    auto outputStream1 = new IOutputStream("compressed.txt");
    Encode(*inputStream1, *outputStream1);
    delete inputStream1;
    delete outputStream1;
    auto inputStream = new IInputStream("compressed.txt");
    auto outputStream = new IOutputStream("output.txt");
    Decode(*inputStream, *outputStream);
    delete inputStream;
    delete outputStream;
}