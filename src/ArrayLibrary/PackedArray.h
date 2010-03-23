/*******************************************************************************
 Copyright (c) 2008-9 Oliver Serang

 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use,
 copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following
 conditions:

 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.

 $Id: PackedArray.h,v 1.3 2009/01/09 14:41:00 lukall Exp $

 *******************************************************************************/

#ifndef _PackedArray_H
#define _PackedArray_H

#include "OrderedArray.h"

using namespace std;

template <typename T>
struct PackedPair {
  PackedPair() {}
  PackedPair(int i, const T & v) {
    index = i;
    value = v;
  }
  int index;
  T value;
};

operator < (const PackedPair & lhs, const PackedPair & rhs) {
  return lhs.index < rhs.index;
}


template <typename T>
class PackedArray : public OrderedArray< PackedPair<T> > {
public:
  virtual const T & operator [] (int k) const;
  virtual T & operator [] (int k);
  virtual void push_back(const PackedPair<T> & element);
protected:

};

#include "PackedArray.cpp"

#endif

