/** Test harness for TexPHPUtil::DeserialisePHPObject().
 */
#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>
#include <typeinfo>
#include <cstdlib>
#include <cstdio>
#include "Compiler.h"
#include "PHPUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << ::progname << " serialised_object_input_file\n";
    std::exit(EXIT_FAILURE);
}


void PrintString(const std::shared_ptr<PHPUtil::DataType> &string_candidate, const unsigned indent) {
    const PHPUtil::String * const string(dynamic_cast<PHPUtil::String *>(string_candidate.get()));
    if (unlikely(string == nullptr))
        logger->error("in PrintString: expected PHPUtil::String, found "
                      + std::string(typeid(string_candidate).name()) + " instead!");

    std::cout << std::string(indent, ' ') << "String: " << string->getName() << '(' << string->getValue() << ")\n";
}


void PrintInteger(const std::shared_ptr<PHPUtil::DataType> &integer_candidate, const unsigned indent) {
    const PHPUtil::Integer * const integer(dynamic_cast<PHPUtil::Integer *>(integer_candidate.get()));
    if (unlikely(integer == nullptr))
        logger->error("in PrintInteger: expected PHPUtil::Integer, found "
                      + std::string(typeid(integer_candidate).name()) + " instead!");

    std::cout << std::string(indent, ' ') << "Integer: " << integer->getName() << '(' << integer->getValue() << ")\n";
}


void PrintFloat(const std::shared_ptr<PHPUtil::DataType> &float_candidate, const unsigned indent) {
    const PHPUtil::Float * const flt(dynamic_cast<PHPUtil::Float *>(float_candidate.get()));
    if (unlikely(flt == nullptr))
        logger->error("in PrintFloat: expected PHPUtil::Float, found "
                      + std::string(typeid(float_candidate).name()) + " instead!");

    std::cout << std::string(indent, ' ') << "Float: " << flt->getName() << '(' << flt->getValue() << ")\n";
}


// Forward declaration:
void PrintObject(const std::shared_ptr<PHPUtil::DataType> &object_candidate, const unsigned indent);


void PrintArray(const std::shared_ptr<PHPUtil::DataType> &array_candidate, const unsigned indent) {
    const PHPUtil::Array * const array(dynamic_cast<PHPUtil::Array *>(array_candidate.get()));
    if (unlikely(array == nullptr))
        logger->error("in PrintArray: expected PHPUtil::Array, found " + std::string(typeid(array_candidate).name())
                      + " instead!");

    std::cout << std::string(indent, ' ') << "Array: " << array->getName() << "(size:" << array->size() << ")\n";

    for (auto entry(array->cbegin()); entry != array->cend(); ++entry) {
        std::cout << std::string(indent + 2, ' ') << "Index(" << entry->first << ")\n";
        switch (entry->second->getType()) {
        case PHPUtil::Type::OBJECT:
            PrintObject(entry->second, indent + 2);
            break;
        case PHPUtil::Type::ARRAY:
            PrintArray(entry->second, indent + 2);
            break;
        case PHPUtil::Type::STRING:
            PrintString(entry->second, indent + 2);
            break;
        case PHPUtil::Type::INTEGER:
            PrintInteger(entry->second, indent + 2);
            break;
        case PHPUtil::Type::FLOAT:
            PrintFloat(entry->second, indent + 2);
            break;
        }
    }
}


void PrintObject(const std::shared_ptr<PHPUtil::DataType> &object_candidate, const unsigned indent = 0) {
    const PHPUtil::Object * const object(dynamic_cast<PHPUtil::Object *>(object_candidate.get()));
    if (unlikely(object == nullptr))
        logger->error("in PrintObject: expected PHPUtil::Object, found "
                      + std::string(typeid(object_candidate).name()) + " instead!");

    std::cout << std::string(indent, ' ') << "Object: ";
    const std::string &name(object->getName());
    std::cout << (name.empty() ? "*top level*" : name) << '(' << object->getClass() << ")\n";

    for (auto entry(object->cbegin()); entry != object->cend(); ++entry) {
        switch (entry->second->getType()) {
        case PHPUtil::Type::OBJECT:
            PrintObject(entry->second, indent + 2);
            break;
        case PHPUtil::Type::ARRAY:
            PrintArray(entry->second, indent + 2);
            break;
        case PHPUtil::Type::STRING:
            PrintString(entry->second, indent + 2);
            break;
        case PHPUtil::Type::INTEGER:
            PrintInteger(entry->second, indent + 2);
            break;
        case PHPUtil::Type::FLOAT:
            PrintFloat(entry->second, indent + 2);
            break;
        }
    }
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];
    if (argc != 2)
        Usage();

    const std::string input_filename(argv[1]);
    FILE * const input(std::fopen(input_filename.c_str(), "r"));
    if (input == nullptr)
        logger->error("can't open \"" + input_filename + "\" for reading!");

    const size_t MAX_BUFFER_SIZE(10240);
    char buffer[MAX_BUFFER_SIZE];
    const size_t actual_size(std::fread(buffer, 1, sizeof buffer, input));
    if (std::ferror(input) != 0)
        logger->error("a read error occurred!");
    
    try {
        std::shared_ptr<PHPUtil::DataType> php_object(
            PHPUtil::DeserialisePHPObject(std::string(buffer, actual_size)));
        PrintObject(php_object);
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }

    std::fclose(input);
}
