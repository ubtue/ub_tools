/** \file    StlHelpers.h
 *  \brief   Classes, templates and functions to boost STL power.
 *  \author  Mr. Walter Howard
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Jason Scheirer
 */

/*
 *  Copyright 2006-2009 Project iVia.
 *  Copyright 2006-2009 The Regents of The University of California.
 *  Copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef STL_HELPERS_H
#define STL_HELPERS_H


#include <algorithm>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <cstring>
#include <AutoConvert.h>
#include <PerlCompatRegExp.h>


/** \namespace  StlHelpers
 *
 *  Templates and classes that have proven useful for STL work. For example, in here you would put templatized versions of various
 *  predicates that are widely useful.
 */
namespace StlHelpers {


/** \brief PtrOrRef is a smart pointer with limited utility, holding a pointer to a class object, that tries to act
 *         as if it were a member of the actual class. The standard template library container classes are limited
 *         in what they can hold. We often need to look at a subset of a container but we don't need a separate copy
 *         of the items, just const references to them. Copying heavy items just to peek at them is performance
 *         prohibitive. Unfortunately, the STL containers cannot hold references but they can hold PtrOrRefs that
 *         hold a pointer but act like an instance of the class. IMPORTANT: The PtrOrRef does NOT manage the pointer.
 *         It doesn't create, nor does it destroy its referent.  If you destroy the referent the PtrOrRef will be left
 *         with a dangling pointer. If you destroy the PtrOrRef, there is no clean up necessary.  It is merely useful
 *         because it uses object syntax, while being a pointer.
 *         Most of the algorithms in this file take an PtrOrRef as a parameter. This makes it so that you can pass
 *         either a reference, an instance or a pointer to any of the functions and they will work just the same.
 */
template <typename Class> class PtrOrRef {
public:
    const Class *item_;

    // If copying from another PtrOrRef<Class>, don't PtrOrRef the PtrOrRef, simple extract
    // the Class and create a simple PtrOrRef. Prevents PtrOrRefs of PtrOrRefs.
    inline PtrOrRef(const PtrOrRef<Class> *ptr = NULL) { operator=(ptr->item_); };
    inline PtrOrRef(const PtrOrRef<Class> &ref) { operator=(ref.item_); };

    // Turn a refrence to Class into an PtrOrRef<Class>
    inline const Class &operator=(const Class &rhs) { item_ = &rhs; return *item_;}
    inline PtrOrRef(const Class &item) { operator=(item); };

    // Turn a pointer to Class into an PtrOrRef<Class>
    inline const Class &operator=(const Class *rhs) { item_ = rhs; 	return *item_; }
    inline PtrOrRef(const Class *item = NULL) { operator=(item); }

    inline operator const Class &() const {
        if (item_ == NULL)
            throw std::runtime_error("in PtrOrRef::operator=: PtrOrRef has NULL item!");
        return *item_;
    }

    inline const Class &operator*() const {
        if (item_ == NULL)
            throw std::runtime_error("in PtrOrRef::operator*: PtrOrRef has NULL item!");
        return *item_;}

    inline const Class *operator->() const {
        if (item_ == NULL)
            throw std::runtime_error("in  PtrOrRef::operator->: PtrOrRef has NULL item!");
        return item_;
    }
};


/** Constructing an PtrOrRef directly means you need to specify the class, like PtrOrRef<std::string> aString("test").
 *  This helper function will figure out the class for you automagically, construct an PtrOrRef and return it to you.
 *  Example: return MakePtrOrRef(aString); Because the PtrOrRef contains only a pointer, construction and copying are
 *  virtually CPU free (the cost of assigning a pointer).
 */
template <typename Class> const PtrOrRef<Class> MakePtrOrRef(const Class &item) {
    return PtrOrRef<Class>(item);
}


template <typename Class> const PtrOrRef<Class> MakePtrOrRef(const PtrOrRef<Class> &item) {
    return PtrOrRef<Class>(item.item_);
}


template <typename Type> class MakeFunctorFromConstructor {
public:
    template <typename FromType> Type operator()(const FromType& from_type) { return Type(from_type); }
};


/** \brief Comparison function for std::pairs that returns true if lhs.first < rhs.first.
 *  \note  Useful in STL searches and where comparsions of pairs should be based solely on operator< for the "first"
 *         components of the pairs and "second" should not be taken into consideration. Sometimes pair::second doesn't
 *         have an operator< and it's a hassle to write one.
 */
template<typename PairType> bool CompareFirstOnly(const PairType &lhs, const PairType &rhs) {
    if (lhs.first < rhs.first )
        return true;

    return false;
}


/** \brief Comparison function for std::pairs that returns true if lhs.first < rhs.first.
 *  \note Useful in STL searches and where comparsions of pairs should be based solely on operator< for the "first"
 *        components of the pairs and "second" should not be taken into consideration.
 */
template<typename PairType> bool CompareBySecondThenFirst(const PairType &lhs, const PairType &rhs) {
    if (lhs.second < rhs.second )
        return true;

    if (lhs.second > rhs.second )
        return false;

    if (lhs.first < rhs.first )
        return true;

    return false;
}


/**
 * Helps bridge the gap between sequence containers and associative containers. If passed a scalar value, will just
 * return the value, but if passed an std::pair, will return the second element.  This is to help you write predicates
 * that can work on both elements of associate containers and containers of pairs
 */
template<typename Type> inline const Type &Value(const Type &value) {
    return value;
}


template<typename Type> inline Type &Value(Type &value) {
    return value;
}


template<typename First, typename Second> inline const Second &Value(const std::pair<First, Second> &pair) {
    return pair.second;
}


template<typename First, typename Second> inline Second &Value(std::pair<First, Second> &pair) {
    return pair.second;
}


template<typename Type> inline const Type &Key(const Type &key) {
    return key;
}


template<typename First, typename Second> inline const First &Key(const std::pair<First, Second> &pair) {
    return pair.first;
}


template<typename Type> inline Type &Key(Type &key) {
    return key;
}


template<typename First, typename Second> inline First &Key(std::pair<First, Second> &pair) {
    return pair.first;
}


/**
 *  These simulate "template typedefs". Value<aType> will create a member typedef. Value::Type will be the element
 *  type for normal containers. Value::Type will be the type of the second member of pair for containers of pairs.
 */
template <typename Class> class ValueType {
public:
	typedef Class Type;
};


template <typename First, typename Second> class ValueType<std::pair<First, Second> > {
public:
    typedef Second Type;
};


/** Bring std::operator<(const std::pair<> &, const std::pair<> &) into the global namespace. */
template <typename First, typename Second> inline bool operator<(const std::pair<First, Second> &lhs,
                                                                 const std::pair<First, Second> &rhs)
{
    return std::operator<(lhs, rhs);
}


/** Creates a pair from a string with some delimiters. Forms a basis for a class that constructs pairs of whatever,
 *  from a string. The first word in the string becomes the key and the remaining text becomes the value. From the
 *  source string, a pair of "FirstType", "SecondType" is created. The pair can consist of any types that AutoConvert
 *  can convert to, so that a string like "population=12000" will construct a pair with a string key, and an int
 *  value. This can then be used wherever an std::pair is used.
 *
 *  The PairFromString being created can consist of any two types. To turn the string elements which are derived from
 *  "source" into the proper types, a Convert template parameter can be specified. The default one works pretty well
 *  for converting from std::string to other built in types.  If you get more complicated than that, you must provide
 *  a functor class whose operator() takes an std::pair<const char *  *, const char *> and returns and std::pair of
 *  the types that go into your PairFromString. For an example, see the AutoConvertPair() above which is the default
 *  Converter.
 */
template <typename FirstType = std::string, typename SecondType = std::string,
          typename Converter = AutoConvertPair<FirstType, SecondType> >
class PairFromString: public std::pair<FirstType, SecondType> {
    Converter converter_;
    typedef std::pair<const char *, const char *> CstringPair;
public:
    /** \brief  Creates from a string broken on delimiting characters using std::strtok_r
     *  \param  source            The string to be broken.
     *  \param  delimiting_chars  The characters that will be considered delimiters
     */
    explicit PairFromString(const std::string &source, const std::string &delimiting_chars = " \t\r\n/.:=") {
        char *splitee(strdupa(source.c_str()));
        const char * const delimiters = delimiting_chars.c_str();
        char *position_save(splitee);

        char *first_delimiter = ::strtok_r(splitee, delimiters, &position_save);

        // No delimiter? return an empty pair
        if (unlikely(first_delimiter == NULL))
            throw std::runtime_error("in PairFromString::PairFromString: No delimiters \"" + delimiting_chars
                                     + "\" found in \"" + source + "\"!");

        const char * const first_string = first_delimiter;

        // Increment past any delimiters:
        position_save += std::strspn(position_save, delimiting_chars.c_str());

        const char * const second_string = position_save;

        set_pair(first_string, second_string);
    }

    /**  \brief  Split a string into two elements by regular expression.
     *   \note The regular expression must have two submatches, designated using parentheses () (). The parts of source which match what
     *         is in the parentheses will be the first and second elements of the pair.  Example, to split on intervening whitespace
     *         or equal sign: PairFromString<std::string, int> thePair("value = 1", "(\\S+)[ =]+(.*)");
     */
    PairFromString(const std::string &source, const PerlCompatRegExp &splitter) {
        if (not splitter.match(source))
            throw std::runtime_error("in PairFromString::PairFromString: Pattern " + splitter.getPattern()
                                     + " not found in string " + source + "!");

        unsigned substring_count(splitter.getSubstringMatchCount());
        if (substring_count < 2)
            throw std::runtime_error("PairFromString::PairFromString: Not enough substrings found matching pattern "
                                     + splitter.getPattern() + " to string " + source + "!");

        set_pair(splitter.getMatchedSubstring(1).c_str(), splitter.getMatchedSubstring(2).c_str());
    }
private:
    void set_pair(const char * const first_string, const char * const second_string) {
        const std::pair<FirstType, SecondType> new_pair(converter_(CstringPair(first_string, second_string)));
        // Necessary to override bug in std::map where map::value_type::first_type is const even though the value is not actually in
        // the map yet being used as the key. No reason for map::value_type::first_type to be const until it is actually in the map.
        *const_cast<FirstType *>(&this->first) = new_pair.first;
        this->second = new_pair.second;
    }
};


/**
 *  Make a pointer, look like a reference. Needed to make "containers of pointers" which look and act like containers of the actual objects.
 */
template <typename Type> class InstanceOrPointer
{
protected:
	const Type *referent_;
public:
	InstanceOrPointer &operator=(const Type *rhs) { referent_ = rhs; return *this; }
	InstanceOrPointer &operator=(const Type &rhs) { referent_ = &rhs; return *this; }
	InstanceOrPointer(const Type *referent = NULL) { operator=(referent); }
	InstanceOrPointer(const Type &referent) { operator=(&referent); }
	operator const Type &() const { return *referent_; }
	const Type &operator->() { return *referent_; }
	const Type &operator*() { return *referent_; }
};


/** \brief Extracts the Values (pair::second) from an entire map style object. Also works for any STL container that holds std::pairs
 *  \note  Example:
 *
 *         std::map<std::string, unsigned> StringNumbers;
 *         StringNumbers["one"] = 1;
 *         StringNumbers["two"] = 2;
 *         StringNumbers["three"] = 3;
 *         StringNumbers["four"] = 4;
 *         std::vector<unsigned> Numbers;
 *         Values(StringNumbers.begin(), StringNumbers.end(), std::back_inserter(Numbers));
 *         // Numbers would now contain 1, 2, 3, 4.
 *
 *         Other templates which follow are various permutations of this same idea:
 *         Values(StringNumbers.begin(), StringNumbers.end(), &Numbers);
 *         Numbers = Values(StringNumbers.begin(), StringNumbers.end());
 *         Numbers = Values(StringNumbers);
 *
 *         The Keys() functions do the same thing but take the key value of the map element (pair::first)
 */
template<typename OutputIterator, typename ForwardIterator>
void Values(const ForwardIterator &first, const ForwardIterator &last, OutputIterator output)
{
	for (ForwardIterator member(first); member != last; ++member)
		*output = Value(*member);
}


template<typename ForwardIterator>
std::vector<typename ForwardIterator::value_type::second_type> Values(const ForwardIterator &first, const ForwardIterator &last)
{
	typedef std::vector<typename ForwardIterator::value_type::second_type> OutputContainer;
	OutputContainer output;
	Values(first, last, std::inserter(output, output.end()));
	return output;
}


template<typename MapStyleContainer>
std::vector<typename MapStyleContainer::value_type::second_type> Values(const MapStyleContainer &container)
{
	typename MapStyleContainer::const_iterator first(container.begin());
	typename MapStyleContainer::const_iterator last(container.end());

	return Values(first, last);
}


template<typename OutputIterator, typename ForwardIterator>
void ValuesPointers(const ForwardIterator &first, const ForwardIterator &last, OutputIterator output)
{
	for (ForwardIterator member(first); member != last; ++member)
		*output = &Value(*member);
}

template<typename ForwardIterator>
std::vector<typename ForwardIterator::value_type::second_type *> ValuesPointers(const ForwardIterator &first, const ForwardIterator &last)
{
	typedef std::vector<typename ForwardIterator::value_type::second_type *> OutputContainer;
	OutputContainer output;
	ValuesPointers(first, last, std::inserter(output, output.end()));
	return output;
}


template<typename MapStyleContainer>
std::vector<const typename MapStyleContainer::value_type::second_type *> ValuesPointers(const MapStyleContainer &container)
{
	typename MapStyleContainer::const_iterator first(container.begin());
	typename MapStyleContainer::const_iterator last(container.end());

	return ValuesPointers(first, last);
}


/**
   The UnconstMaker class and the Unconst macro work together to "Unconst" a type. Note, we are talking about a type, not a variable. To
   unconst a variable you would use const_cast. This simulates what might be thought of as an unconst keyword providing the reverse of the
   const keyword.

   Example:

   typedef const int ConstNumber; // A const type
   typedef Unconst(ConstNumber) UnconstNumber;  // Take that type and make an unconst version of it.
*/
template <typename UnconstMe> class UnconstMaker {
	template <typename U>
	struct UnConst {
		typedef U Unconsted;
	};
	template <typename U>
	struct UnConst <const U> {
		typedef U Unconsted;
	};
public:
	typedef typename UnConst<UnconstMe>::Unconsted NonConst;
};


#define Unconst(CONST_TYPE) UnconstMaker<CONST_TYPE>::NonConst


template<typename OutputIterator, typename ForwardIterator>
void Keys(const ForwardIterator &first, const ForwardIterator &last, OutputIterator output)
{
	std::copy(first, last, output);
}


template<typename ForwardIterator>
std::vector<typename ForwardIterator::value_type::first_type> Keys(const ForwardIterator &first, const ForwardIterator &last)
{
	typedef typename ForwardIterator::value_type::first_type ElementType;
	typedef std::vector< typename UnconstMaker<ElementType>::NonConst > OutputContainer;
	OutputContainer output;
	Keys(first, last, std::inserter(output, output.end()));
	return output;
}


template<typename MapStyleContainer>
const std::vector<typename MapStyleContainer::value_type::first_type> Keys(const MapStyleContainer &container)
{
	typename MapStyleContainer::const_iterator first(container.begin());
	typename MapStyleContainer::const_iterator last(container.end());

	return Keys(first, last);
}


/** \brief Functor that adds operator() arguments to a total;
 *
 * \note  Example: return StlHelpers::Totalizer total; return for_each(container.begin(), container.end(), total); the operator NumberType()
 *        lets you return the total value as the result of the STL algorithm you used, in the example above for_each.
 */
template <typename ContainerElement> class Totalizer: public std::unary_function<void, typename ValueType<ContainerElement>::Type> {
	typedef typename ValueType<ContainerElement>::Type NumberType;
	NumberType total_;
public:
	Totalizer() : total_(NumberType()) { }
	void operator()(const ContainerElement &item)
	{
		NumberType val = StlHelpers::Value(item);
		total_ += val;
	}
	operator NumberType &() { return total_; } // Allows interpreting a Totalizer as a NumberType
};


template<typename IteratorType> typename ValueType<typename IteratorType::value_type>::Type Total(const IteratorType &first, const IteratorType &last)
{
	Totalizer<typename IteratorType::value_type> totalizer;
	return std::for_each(first, last, totalizer);
}


template<typename StlContainer> typename ValueType<typename StlContainer::value_type>::Type Total(const StlContainer &container)
{
	Totalizer<typename StlContainer::value_type> totalizer;
	return std::for_each(container.begin(), container.end(), totalizer);
}


/** Turn a reference to a pointer into just the pointer. */
template<typename ItemType> class Deref {
	ItemType *item_;
public:
	explicit Deref(const ItemType *item): item_(item) { }
	operator ItemType *() const { return item_; }
};


template<typename ForwardIterator, typename ValueType, typename Comparator>
ForwardIterator BinarySearch(const ForwardIterator &first, const ForwardIterator &last, const ValueType &value_to_match, const Comparator &comp)
{
	const ForwardIterator result(std::lower_bound(first, last, value_to_match, comp));
	if (result == last)
		return last;

	// (not a < b) and (not b < a) => a == b:
	const bool equal(not comp(*result, value_to_match) and not comp(value_to_match, *result));
	return equal ? result : last;
}


template<typename ForwardIterator, typename ValueType>
inline ForwardIterator BinarySearch(const ForwardIterator &first, const ForwardIterator &last, const ValueType &value_to_match)
{
	return BinarySearch(first, last, value_to_match, std::less<ValueType>());
}


template<typename Container, typename ValueType>
inline typename Container::const_iterator BinarySearch(const Container &container, const ValueType &value_to_match)
{
	return BinarySearch(container.begin(), container.end(), value_to_match, std::less<ValueType>());
}


/** \brief  This entire series of templates converts a function like "ReturnType Function(Arg1Type arg1, OutputType * output)"
 *          into "OutputType Function(Arg1Type arg1)". It returns what normally would be an "output" variable.
 *  \code   Example: "Adapting ToDouble to return the double being converted to"
 *          bool ToDouble(const std::string &s, double * const n);
 *          Normally you'd have to do this:
 *
 *          double value;
 *          StringUtil::ToDouble("23.5", *value);
 *          std::cout << value;
 *
 *          With the adapter OutToReturn you can just do this:
 *          std::cout << OutToReturn(StringUtil::ToDouble, "23.5");
 *  \endcode
 */

/** \brief  Converts a function like "ReturnType Function(OutputType * output)" into "OutputType Function()".
 */
template <typename ResultType, typename OldReturnType>
inline ResultType OutToReturn(OldReturnType (*function)(ResultType * const))
{
	ResultType temp;
	function(&temp);
	return temp;
}

/** \brief  Converts a function like "ReturnType Function(Argument, OutputType * output)" into "OutputType Function(Argument)".
 */
template <typename Arg1, typename ResultType, typename OldReturnType>
inline ResultType OutToReturn(OldReturnType (*function)(Arg1, ResultType * const), Arg1 arg)
{
	ResultType temp;
	function(arg, &temp);
	return temp;
}


/** \brief  Converts a function like "ReturnType Function(Argument1, Argument2, OutputType * output)" into "OutputType Function(Argument1, Argument2)".
 */
template <typename Arg1, typename Arg2, typename ResultType, typename OldReturnType>
inline ResultType OutToReturn(OldReturnType (*function)(Arg1, Arg2, ResultType * const), Arg1 arg1, Arg2 arg2)
{
	ResultType temp;
	function(arg1, arg2, &temp);
	return temp;
}


/** \brief  Outputs something to a stream in string form.
 *  \param  item              The item that we are streaming.
 *  \param  output            The stream ( anything that understands operator<<() ) to send the output to.
 *  \param  member_delimiter  The string to put between each container element during out.
 */
template <typename ItemType, typename OutputStream, typename MemberDelimiter>
inline void ItemToStream(const ItemType &item, OutputStream &output, MemberDelimiter member_delimiter, const std::string & = "")
{
	output << member_delimiter << item;
}


/** \brief  Outputs something to a stream in string form. Returns the count of objects.
 *  \param  item              The item that we are streaming.
 *  \param  output            The stream ( anything that understands operator<<() ) to send the output to.
 *  \param  member_delimiter  The string to put between each container element during out.
 */
template <typename ItemType, typename OutputStream, typename MemberDelimiter>
inline void ItemToStream(const PtrOrRef<ItemType> item, OutputStream &output, MemberDelimiter &member_delimiter, const std::string & = "")
{
	ItemToStream(*item, output, member_delimiter);
}


/** \brief  Outputs a pair to a stream in string form. Returns the count of objects.
 *  \param  item              The pair to output.
 *  \param  output            The stream ( anything that understands operator<<() ) to send the output to.
 *  \param  member_delimiter  The string to put between each container element during out.
 *  \param  pair_delimiter    If the elements are pairs, put this delimiter between first and second.
 */
template <typename FirstType, typename SecondType, typename OutputStream, typename MemberDelimiter>
void ItemToStream(const PtrOrRef<std::pair<FirstType, SecondType> > item, OutputStream &output, MemberDelimiter &member_delimiter,
		  const std::string &pair_delimiter = "")
{
	ItemToStream(item->first, output, member_delimiter);
	output << pair_delimiter;
	ItemToStream(item->second, output, "");
}


/** \brief                    Prints container elements, as strings, to a stream.
 *  \param  container         The stl container.
 *  \param  output            The stream ( anything that understands operator<<() ) to send the output to.
 *  \param  member_delimiter  The string to put between each container element during out.
 *  \param  pair_delimiter    If the elements are pairs, put this delimiter between first and second.
 */
template <typename ContainerType, typename OutputStream, typename MemberDelimiter>
unsigned ContainerToStream(const ContainerType &container, OutputStream &output, MemberDelimiter &member_delimiter,
			   const std::string &pair_delimiter = ":")
{
	for (typename ContainerType::const_iterator item(container.begin()); item != container.end(); ++item) {
		if (item == container.begin())
			ItemToStream(MakePtrOrRef(*item), output, "", pair_delimiter);
		else
			ItemToStream(MakePtrOrRef(*item), output, member_delimiter, pair_delimiter);
	}
	return container.size();
}


/** \brief copy from one container to another performing a conversion */
template <typename SourceContainerType, typename DestinationContainerType, typename Converter>
void ConvertCopy(const SourceContainerType &source, DestinationContainerType * const destination,
                 Converter converter = AutoConvertNull())
{
    for (typename SourceContainerType::const_iterator item(source.begin()); item != source.end(); ++item)
        destination->insert(destination->end(), converter(*item));
}


/** \brief Finds a map element or returns a default value_type. Can be used where operator[] is not usable because operator[] only works for
 *  non-const maps.
*/
template <typename MapContainerType, typename SearchItemType>
const typename MapContainerType::value_type::second_type
&FindOrDefault(const SearchItemType &find_this, const MapContainerType &container) {
    /**
     * This long winded initialization is needed. The shorter syntax static typename value() doesn't work because the compiler then
     * thinks you are declaring a function.
     */
    static typename MapContainerType::value_type::second_type default_value =
        typename MapContainerType::value_type::second_type();

    typename MapContainerType::const_iterator item = container.find(find_this);
    if (item == container.end())
        return default_value;
    return item->second;
}


/** \brief Finds a map element or throws  */
template <typename MapContainerType, typename SearchItemType>
const typename MapContainerType::value_type::second_type &FindOrThrow(const SearchItemType &find_this,
                                                                      const MapContainerType &container)
{
    typename MapContainerType::const_iterator item = container.find(find_this);
    if (item == container.end())
        throw std::runtime_error("in StlHelpers::FindOrThrow: Unable to find item in container!");
    return item->second;
}


/** \brief Finds a map element or throws  */
template <typename MapContainerType, typename SearchItemType> bool
FindOrFalse(const SearchItemType &find_this, const MapContainerType &container) {
    typename MapContainerType::const_iterator item = container.find(find_this);
    if (item == container.end())
        return false;
    return true;
}


/** Convert an element to a string */
template <typename Element> inline std::string ElementToString(const Element &element) {
    return AutoConvert(element);
}


template <typename First, typename Second> std::string ElementToString(const std::pair<First, Second> &element) {
    std::string rval(ElementToString(element.first));
    rval += ':';
    rval += ElementToString(element.second);
    return rval;
}


/**
 *  Return a vector of PtrOrRefs to elements of StlContainer that match regex. Converts elements to strings to perform the match (separate
 *  variable doesn't alter the original).
 */
template <typename StlContainer> std::vector<PtrOrRef<typename StlContainer::value_type> >
GrepContainerPtr(const StlContainer &source, const PerlCompatRegExp &regex) {
    std::vector<PtrOrRef<typename StlContainer::value_type> > matches;

    for (typename StlContainer::const_iterator item(source.begin()); item != source.end(); ++item)
        if (regex.match(ElementToString(*item)))
            matches.insert(matches.end(),  &*item);

    return matches;
}


template <typename StlContainer> std::vector<PtrOrRef<typename StlContainer::value_type> >
GrepContainerPtrNot(const StlContainer &source, const PerlCompatRegExp &regex) {
    std::vector<PtrOrRef<typename StlContainer::value_type> > matches;

    for (typename StlContainer::const_iterator item(source.begin()); item != source.end(); ++item)
        if (not regex.match(ElementToString(*item)))
            matches.insert(matches.end(),  &*item);

    return matches;
}


/** \brief  Return a subset of StlContainer consisting of elements that match regex.
 *          Converts elements to strings to perform the match (does not alter the original). If the element is a pair, it will join the
 *          string versions of each element with a : between, like population:23431. This allows you to match the regex on the entire pair.
 */
template <typename StlContainer> StlContainer GrepContainer(const StlContainer &source, const PerlCompatRegExp &regex)
{
    StlContainer matches;

    for (typename StlContainer::const_iterator item(source.begin()); item != source.end(); ++item) {
        if (regex.match(ElementToString(*item)))
            matches.insert(matches.end(),  *item);
    }

    return matches;
}


template <typename StlContainer> StlContainer GrepContainerNot(const StlContainer &source,
                                                               const PerlCompatRegExp &regex)
{
    StlContainer matches;

    for (typename StlContainer::const_iterator item(source.begin()); item != source.end(); ++item)
        if (not regex.match(ElementToString(*item)))
            matches.insert(matches.end(),  *item);

    return matches;
}


/** \brief  Return a subset of StlContainer consisting of elements that match regex.
 *          Converts elements to strings to perform the match (does not alter the original). If the element is a
 *          pair, it will join the string versions of each element with a : between, like population:23431. This
 *          allows you to match the regex on the entire pair.
 */
template <typename First, typename Second>
std::unordered_map<First, Second> GrepContainer(const std::unordered_map<First, Second> &source,
                                                const PerlCompatRegExp &regex)
{
    std::unordered_map<First, Second> matches;
    for (typename std::unordered_map<First, Second>::const_iterator item(source.begin()); item != source.end();
         ++item)
    {
        if (regex.match(ElementToString(*item)))
            matches.insert(*item);
    }

    return matches;
}


/** \brief  Join words iterated by first to last to form a single string.
 *  \param  first      Iterator to the starting word of those to be combined.
 *  \param  last       Iterator one past the last word to be combined.
 *  \param  separator  Stick this between each word
 *  \return The number of strings that were joined.
 *
 *  Joins container elements pointed to by iterators over a range with separator between them and returns the resultant
 *  string. Because it uses AutoConvert it will also work on iterators to other primitive types, turning them
 *  into strings before joining them.
 */
template <typename ForwardIterator> std::string ContainerToString(ForwardIterator first, ForwardIterator last,
                                                                  const std::string &separator = "")
{
    std::string resultant_string;
    while (first != last) {
        const std::string first_string = AutoConvert(*first);
        resultant_string += first_string;
        ++first;
        if (first != last)
            resultant_string += separator;
    }
    return resultant_string;
}


/** \brief  Sort a container
 *  \note   Convenience wrapper for std::sort()
 */
template <typename StlContainer, typename CompareFunction>
inline StlContainer &SortContainer(StlContainer &container,
                                   CompareFunction compare = std::less<typename StlContainer::value_type>())
{
    std::sort(container.begin(), container.end(), compare);
    return container;
}


template <typename StlContainer> StlContainer &SortContainer(StlContainer &container)
{
    std::sort(container.begin(), container.end());
    return container;
}


/** \brief  A functor that returns whether a container's size is less than a given cutoff size or not. */
class SmallerThan {
    size_t cutoff_size_;
public:
    SmallerThan(const size_t &cutoff_size): cutoff_size_(cutoff_size) { }
    template <typename Container> bool operator()(const Container &comparee) const
        { return comparee.size() < cutoff_size_; }
};


/** \brief  Determines membership in a container. */
template<typename Container> inline bool Contains(const Container &container,
                                                  const typename Container::value_type entry_candidate)
{
    return std::find(container.begin(), container.end(), entry_candidate) != container.end();
}


/** \brief some very rudimentary functional-programming inspired templates */
namespace Functional {
    /** \brief wrapper for function pointers in foreach */
    template<typename ArgumentType> class ForEachFunctionPointer {
        void (*function_)(ArgumentType);
    public:
        ForEachFunctionPointer(void (*function_pointer)(ArgumentType)) : function_(function_pointer) { }
        void operator()(ArgumentType argument) {
            function_(argument);
        }
    };

    template<typename Container, typename ArgumentType>
    inline void ForEachFunction(const Container &container, void (*function)(ArgumentType)) {
        std::for_each(container.begin(), container.end(), ForEachFunctionPointer<ArgumentType>(function));
    }

    /** \brief wrapper for function pointers in apply */
    template<typename ArgumentType, typename ReturnType>
    class Applicator {
	ReturnType (*function_)(ArgumentType);
    public:
	Applicator(ReturnType (*function_pointer)(ArgumentType)) : function_(function_pointer) { }
	ReturnType operator()(ArgumentType argument) {
            return function_(argument);
	}
    };

    /** \brief Inserts ApplyFunction(element) to new_container for each element in original container */
    template<typename ContainerIterator, typename ValueType, typename InsertableContainer, typename ApplyFunction>
    inline void Apply(const ContainerIterator &start, const ContainerIterator &end,
                      InsertableContainer &new_container, ApplyFunction &apply)
    {
        for (ContainerIterator iterator(start); iterator != end; ++iterator)
            new_container.insert(apply(*iterator));
    }

    template<typename Container, typename NewContainer, typename OriginalType, typename ReturnType>
    inline void ApplyFunction(const Container &container, NewContainer &new_container,
                              ReturnType (*function)(OriginalType))
    {
        Apply(container.begin(), container.end(), new_container, Applicator<OriginalType, ReturnType>(function));
    }

    template<typename ArgumentType>
    class Filterer : public Applicator<ArgumentType, bool> { };

    /** \brief Inserts a copy of every element in the original container into the new container
        if FilterFunction(element) returns true for it. */
    template<typename ContainerIterator, typename InsertableContainer, typename FilterFunction>
    inline void Filter(const ContainerIterator &start, const ContainerIterator &end,
                       InsertableContainer &new_container, FilterFunction &filter)
    {
        for (ContainerIterator iterator(start); iterator != end; ++iterator) {
            if (filter(*iterator))
                new_container.insert(*iterator);
        }
    }

    template<typename Container, typename NewContainer, typename OriginalType>
    inline void FilterFunction(const Container &container, NewContainer &new_container,
                               bool (*function)(OriginalType))
    {
        Filter(container.begin(), container.end(), new_container, Filterer<OriginalType>(function));
    }

    /** \brief wrapper for function pointers in collect*/
    template<typename ReturnType> class Collector {
        ReturnType (*function_)(ReturnType, ReturnType);
    public:
        Collector(ReturnType *(function_pointer)(ReturnType, ReturnType)) : function_(function_pointer) { }
        ReturnType operator()(ReturnType A, ReturnType B) {
            return function_(A, B);
        }
    };

    /** \brief Curries elements with the provided binary function */
    template<typename ContainerIterator, typename ValueType, typename CollectFunction>
    inline ValueType Collect(const ContainerIterator &start, const ContainerIterator &end, CollectFunction &collect) {
        ValueType return_value(*start);
        if (start == end)
            return *return_value;
        ContainerIterator iterator(start);
        for (++iterator; iterator != end; ++iterator)
            return_value = collect(return_value, *iterator);
        return return_value;
    }

    template<typename Container, typename OriginalType>
    inline OriginalType CollectFunction(const Container &container,
                                        OriginalType (*function)(OriginalType, OriginalType))
    {
        Collect(container.begin(), container.end(), Collector<OriginalType>(function));
    }
}


    template<typename In, typename Out, typename Predicate> inline Out CopyIf(In first, In last, Out result,
                                                                              Predicate predicate)
    {
	while (first != last) {
            if (predicate(*first))
                *result++ = *first;
            ++first;
	}

	return result;
}


} // namespace StlHelpers


#endif // STL_HELPERS_H
