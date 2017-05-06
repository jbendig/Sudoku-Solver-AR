// Copyright 2017 James Bendig. See the COPYRIGHT file at the top-level
// directory of this distribution.
//
// Licensed under:
//   the MIT license
//     <LICENSE-MIT or https://opensource.org/licenses/MIT>
//   or the Apache License, Version 2.0
//     <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>,
// at your option. This file may not be copied, modified, or distributed
// except according to those terms.

#ifndef COMHELP_H
#define COMHELP_H

#include <memory>

#ifdef _WIN32
template <class T>
using ComPtr = std::unique_ptr<T,void(*)(T*)>;

template <class T>
void ComDestructor(T* ptr)
{
	ptr->Release();
}

template <class T>
ComPtr<T> NullComPtr()
{
	return ComPtr<T>(nullptr,&ComDestructor<T>);
}

template <class T>
ComPtr<T> MakeComPtr(T* t)
{
	return ComPtr<T>(t,&ComDestructor<T>);
}

#define TRY_COM(expr) {const HRESULT hr = expr; if(FAILED(hr)) return {};}
#define TRY_COM_BOOL(expr) {const HRESULT hr = expr; if(FAILED(hr)) return false;}
#endif

#endif

