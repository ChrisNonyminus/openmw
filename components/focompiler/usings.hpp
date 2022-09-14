#ifndef FOCOMPILER_USINGS_H
#define FOCOMPILER_USINGS_H

// header including headers from "compiler" that don't need to have a "clone" class in FOCompiler
// "borrows" the classes from the Compiler namespace into the FOCompiler namespace by using the "using" keyword

#include <string>

#include <components/compiler/context.hpp>
#include <components/compiler/extensions.hpp>
#include <components/compiler/literals.hpp>

namespace FOCompiler
{
    using Context = Compiler::Context;
    using Extensions = Compiler::Extensions;
    using Literals = Compiler::Literals;
}

#endif
