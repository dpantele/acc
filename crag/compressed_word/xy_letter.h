/**
* @file
*
* @brief Definition of a basic letter for CWord
*/
#include <cassert>
#include <ostream>
#include <stdexcept>

#pragma once
#ifndef _CRAG_XY_LETTER_H_
#define _CRAG_XY_LETTER_H_

//! Small store which stores a x, y, X or Y
/**
* Mainly was created to have normal print and invert functions
*/
class XYLetter {
 public:
  static const unsigned short kAlphabetSize = 2;

  template<
    typename T,
    typename = typename std::enable_if<
      std::is_integral<T>::value
      && !std::is_same<typename std::remove_const<typename std::remove_cv<T>::type>::type, char>::value>::type
  >
  /*implicit*/ constexpr XYLetter(T letter)
    : letter_(static_cast<unsigned short>(letter))
  { }

  explicit constexpr XYLetter(char letter)
    : letter_(CharToShort(letter))
  { }

  inline constexpr char AsChar() const {
    switch(letter_) {
      case 0:
        return 'x';
      case 1:
        return 'X';
      case 2:
        return 'y';
      case 3:
        return 'Y';
      default:
        break;
    }

    return '\0';
  }

  inline constexpr unsigned int AsInt() const {
    return letter_;
  }

  XYLetter& operator++() {
    ++letter_;
    return *this;
  }

  inline constexpr XYLetter Inverse() const {
    return XYLetter(static_cast<unsigned short>(letter_ ^ 1));
  }

  constexpr XYLetter(const XYLetter& other)
    : letter_(other.letter_)
  { }

  XYLetter& operator=(const XYLetter& other) {
    letter_ = other.letter_;
    return *this;
  }

 private:
  unsigned short letter_;

  static constexpr inline unsigned short CharToShort(char letter) {
    switch(letter) {
      case 'X':
        return 1;
      case 'y':
        return 2;
      case 'Y':
        return 3;
      default:
        //some strange form so that constexpr variant compiles
        return (letter == 'x') ? 0 : throw std::invalid_argument("Only x, y, X, Y can be transformed into XYLetter");
    }
  }
};

//! Comparison operator to utilize implicit constructors
inline constexpr bool operator==(const XYLetter& lhs, const XYLetter& rhs) {
  return lhs.AsInt() == rhs.AsInt();
}

//! Comparison operator to utilize implicit constructors
inline constexpr bool operator!=(const XYLetter& lhs, const XYLetter& rhs) {
  return lhs.AsInt() != rhs.AsInt();
}

inline constexpr bool operator<(const XYLetter& lhs, const XYLetter& rhs) {
  return lhs.AsInt() < rhs.AsInt();
}


inline std::ostream& operator<<(std::ostream& out, const XYLetter& l) {
  return out << l.AsChar();
}

#endif //_CRAG_XY_LETTER_H_
