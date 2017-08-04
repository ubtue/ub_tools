#ifndef AUTO_CONVERT
#define AUTO_CONVERT


#include <string>
#include <StringUtil.h>


/** \brief Class to convert from any type built-in type to any other built-in type or std::string
 *
 * \note This makes use of "return type overloading". The conversion operators make the AutoConvert interval value
 *       (string pointer) and change the data into the type that is wanted by the context where it is used.
 *       Example: functionThatTakesADouble(AutoConvert("27")) or functionThatTakesAString(AutoConvert(42));
 *       The point is, AutoConvert converts to whatever is wanted in the context you are using it.  You don't have to
 *       know the type explicitly. Why is this necessary? Certain templates require the ability to change a string
 *       into just about any other format without knowing ahead of time what that format is. For example creating a
 *       std::pair from a string of type "key=value".  Depending on what the "pair" is, is defined to be, maybe
 *       std::pair<int,double>, "key=value" (a string) must be converted into int and double respectively. If this is
 *       done in a template, there is no way to know ahead of time what specific "StringTo()" function to call. The
 *       AutoConvert figures that out at compile or run time as necessary.
 *
 *       For efficiency sake this uses a simple char buffer and is thus even faster than a function which returns a
 *       std::string since this one has NO heap allocation. Since it's in a completely private context no problems
 *       can be caused by the users of this class.
 *
 *       Usage:
 *       The AutoConvert can be used in most places where you'd use a To* style function. StringToSomething,
 *       NumberToString etc. You should use it only with function style semantics. That is, don't create instances of
 *       AutoConvert for use later, use it immediately where you want the conversion done. Never return AutoConvert
 *       from a function. Since it contains a pointer to a string potentially outside itself. That referent might be
 *       destroyed, leaving the AutoConvert::string_pointer_ pointing at invalid memory.
 *
 *       Good Examples:
 *       std::vector<int> test; test.insert(AutoConvert("22"));
 *       std::vector<std::string> test; test.insert(AutoConvert(3453));
 *
 *       operator()
 *
 *       AutoConvert has operator()(const TypeToConvertFrom &item). This allows an instance of AutoConvert to be used
 *       in a template class as a functor.  All it does is set the interal values as would the AutoConvert
 *       constructor. STL functor semantics require this function.
 */
class AutoConvert {
    char string_value_[24]; // Only used if constructing from a number. Holds the string version of the number.
    const char *string_pointer_;  // Points at the string data being converted. Sometimes that is "string_value_[]"
                                  // above.
    /**
     * Prohibit creating on the heap. Since AutoConvert potentially contains a pointer to an external string, keeping
     * it around longer than necessary increases potential for dangling pointer.
     */
    static void *operator new(size_t size);
    static void operator delete(void *ptr);
public:
    AutoConvert& operator=(const AutoConvert& rhs) {
        // If we are storing a local conversion in the string buffer
        if (rhs.string_pointer_ == rhs.string_value_) {
            std::strcpy(string_value_, rhs.string_value_);
            string_pointer_ = string_value_;
        }
        else
            string_pointer_ = rhs.string_pointer_;
        return *this;
    }

    AutoConvert(const AutoConvert& rhs) {
        *this = rhs;
    }

    /** \brief   Needed to use an AutoConvert as a functor.
     *
     */
    template <typename Type> AutoConvert &operator()(Type &item) {
        *this = AutoConvert(item);
        return *this;
    }

    explicit AutoConvert(const char single_character): string_pointer_(string_value_) {
        string_value_[0] = single_character;
        string_value_[1] = '\0';
    }

    explicit AutoConvert(const char * const cstring = "") : string_pointer_(cstring) { }

    explicit AutoConvert(const std::string &string_value) : string_pointer_(string_value.c_str()) { }

    explicit AutoConvert(const double number_value) : string_pointer_(string_value_) {
        StringUtil::FastFormat(string_value_, sizeof(string_value_) - 1, "%.16g",
                               static_cast<double>(number_value));
    }

    explicit AutoConvert(const unsigned long long number_value) : string_pointer_(string_value_)
       { StringUtil::FastFormat(string_value_, sizeof(string_value_) - 1, "%llu", number_value); }

    explicit AutoConvert(const long long number_value) : string_pointer_(string_value_)
        { StringUtil::FastFormat(string_value_, sizeof(string_value_) - 1, "%lld", number_value); }

    explicit AutoConvert(const int32_t number_value) : string_pointer_(string_value_)
        { StringUtil::FastFormat(string_value_, sizeof(string_value_) - 1, "%d", number_value); }

    explicit AutoConvert(const uint32_t number_value) : string_pointer_(string_value_)
        { StringUtil::FastFormat(string_value_, sizeof(string_value_) - 1, "%u", number_value); }

    explicit AutoConvert(const long number_value) : string_pointer_(string_value_)
        { StringUtil::FastFormat(string_value_, sizeof(string_value_) - 1, "%ld", number_value); }

    explicit AutoConvert(const unsigned long number_value) : string_pointer_(string_value_)
        { StringUtil::FastFormat(string_value_, sizeof(string_value_) - 1, "%lu", number_value); }

    explicit AutoConvert(const int16_t number_value) : string_pointer_(string_value_)
        { StringUtil::FastFormat(string_value_, sizeof(string_value_) - 1, "%hd", number_value); }

    explicit AutoConvert(const uint16_t number_value) : string_pointer_(string_value_)
        { StringUtil::FastFormat(string_value_, sizeof(string_value_) - 1, "%hu", number_value); }

    explicit AutoConvert(bool bool_value) : string_pointer_(string_value_)
        { StringUtil::FastFormat(string_value_, sizeof(string_value_) - 1, "%s", bool_value ? "1": ""); }

    operator int16_t() const { return static_cast<int16_t>(std::strtol(string_pointer_, NULL, 10)); }
    operator uint16_t() const { return static_cast<uint16_t>(std::strtoul(string_pointer_, NULL, 10)); }
    operator int32_t() const { return static_cast<int32_t>(std::strtol(string_pointer_, NULL, 10)); }
    operator uint32_t() const { return static_cast<uint32_t>(std::strtoul(string_pointer_, NULL, 10)); }
    operator int64_t() const { return std::strtoll(string_pointer_, NULL, 10); }
    operator uint64_t() const { return std::strtoull(string_pointer_, NULL, 10); }
    operator float() const { return StringUtil::ToFloat(string_pointer_); }
    operator double() const { return StringUtil::ToDouble(string_pointer_); }
    operator bool() const { return string_pointer_[0] != '\0'; }

    operator std::string() const { return string_pointer_; }

    /** Note there is no conversion to const char*. This causes some problems because it makes returning a string an ambiguous
        operation. At least the compiler thinks so. */
};


/** Default Converter class for PairFromString */
template <typename First = std::string, typename Second = std::string> class AutoConvertPair {
    typedef std::pair<const char *, const char *> CstringPair;
public:
    /** take an std::pair<const char *, const char *> and convert it to std::pair<First, Second> */
    std::pair<First, Second> operator()(const CstringPair &string_pair) {
        return std::pair<First, Second>(AutoConvert(string_pair.first), AutoConvert(string_pair.second));
    }
};


/** Don't do anything really. Used where a placeholder of some kind of Converter is needed. */
class AutoConvertNull {
public:
    template <typename Type> Type& operator()(Type &item) { return item; }
};


/** Swap elements in an std::pair */
class AutoConvertSwap {
public:
    template <typename First, typename Second> std::pair<Second, First> operator()(const std::pair<First, Second> &item)
    {
        return std::pair<Second, First>(item.second, item.first);
    }
};


#endif // AUTO_CONVERT
