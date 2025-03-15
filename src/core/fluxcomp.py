#!/bin/env python3

license = """
/*
   This file is part of DirectFB.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*/
"""

import argparse

parser = argparse.ArgumentParser(description="Flux Compiler Tool")

parser.add_argument("filename")
parser.add_argument("-c", "--generate-c", action="store_true", required=True, help="Generate C code")
parser.add_argument("-i", "--identity", action="store_true", help="Generate caller identity tracking code")
parser.add_argument("--object-ptrs", action="store_true", required=True, help="Return object pointers rather than IDs")
parser.add_argument("--call-mode", action="store_true", required=True, help="Use call mode function to determine call mode")
parser.add_argument("-p", "--include-prefix", required=True, help="Override standard include prefix for includes")
parser.add_argument("--static-args-bytes", default=1000, help="Override standard limit (1000) for stack based arguments")
parser.add_argument("--dispatch-error-abort", action="store_true", help="Abort execution when object lookups fail")
parser.add_argument("-o", dest="output_dir", default="", help="Write to output directory")

# --------------------------------------------------------------------------------------------------------------------------

class Entity:
    def __init__(self):
        self.position = 0
        self.entities = []

class Interface(Entity):
    def SetProperty(self, name, value):
        if name == "name":
            self.name = value
        elif name == "object":
            self.object = value
            self.dispatch = value
        elif name == "dispatch":
            self.dispatch = value

class Method(Entity):
    sync = True
    queue = False

    def SetProperty(self, name, value):
        if name == "name":
            self.name = value
        elif name == "async":
            self.sync = not value == "yes"
        elif name == "queue":
            self.queue = value == "yes"

    def ArgumentsAsParamDecl(self):
        result = ""
        first = True

        for arg in self.entities:
            result += "" if first else ",\n"

            if arg.type == "struct":
                if arg.direction == "input":
                    result += f"                    {'const ' + arg.type_name:<40}  *{arg.name}"
                else:
                    result += f"                    {arg.type_name:<40}  *ret_{arg.name}"
            elif arg.type == "enum" or arg.type == "int":
                if arg.direction == "input":
                    if arg.array:
                        result += f"                    {'const ' + arg.type_name:<40}  *{arg.name}"
                    else:
                        result += f"                    {arg.type_name:<40}   {arg.name}"
                else:
                    result += f"                    {arg.type_name:<40}  *ret_{arg.name}"
            else:
                if arg.direction == "input":
                    result += f"                    {arg.type_name:<40}  *{arg.name}"
                else:
                    result += f"                    {arg.type_name:<40} **ret_{arg.name}"

            if first:
                first = False

        return result

    def ArgumentsAsMemberDecl(self):
        result = ""

        for arg in self.entities:
            if arg.array:
                continue

            if arg.direction == "input":
                if arg.optional:
                    result += f"    bool                                       {arg.name}_set;\n"

                if arg.type == "struct" or arg.type == "enum" or arg.type == "int":
                    result += f"    {arg.type_name:<40}   {arg.name};\n"
                else:
                    result += f"    u32                                        {arg.name}_id;\n"

        for arg in self.entities:
            if not arg.array:
                continue

            if arg.direction == "input":
                if arg.optional:
                    result += f"    bool                                       {arg.name}_set;\n"

                result += f"    /* '{arg.count}' {arg.type_name} follow ({arg.name}) */\n"

        return result

    def ArgumentsOutputAsMemberDecl(self):
        result = "    DFBResult                                  result;\n"

        for arg in self.entities:
            if arg.array:
                continue

            if arg.direction == "output":
                if arg.type == "struct" or arg.type == "enum" or arg.type == "int":
                    result += f"    {arg.type_name:<40}   {arg.name};\n"
                else:
                    result += f"    u32                                        {arg.name}_id;\n" \
                              f"    void*                                      {arg.name}_ptr;\n"

        for arg in self.entities:
            if not arg.array:
                continue

            if arg.direction == "output":
                result += f"    /* '{arg.count}' {arg.type_name} follow ({arg.name}) */\n"

        return result

    def ArgumentsAsMemberParams(self):
        result = ""
        first = True
        second_output_array = False

        for arg in self.entities:
            if first:
                first = False
            else:
                result += ", "

            if arg.direction == "input":
                if arg.optional:
                    result += f"args->{arg.name}_set ? "

                if arg.array:
                    result += f"({arg.type_name}*) ((char*)(args + 1){arg.offset(self, True, False)})"
                else:
                    if arg.type == "struct":
                        result += f"&args->{arg.name}"
                    elif arg.type == "enum" or arg.type == "int":
                        result += f"args->{arg.name}"
                    else:
                        result += f"{arg.name}"

                if arg.optional:
                    result += " : NULL"

            if arg.direction == "output":
                if arg.array:
                    if second_output_array:
                        result += f"tmp_{arg.name}"
                    else:
                        result += f"({arg.type_name}*) ((char*)(return_args + 1){arg.offset(self, True, True)})"
                        second_output_array = True
                else:
                    if arg.type == "struct" or arg.type == "enum" or arg.type == "int":
                        result += f"&return_args->{arg.name}"
                    else:
                        result += f"&{arg.name}"

        return result

    def ArgumentsInputAssignments(self):
        result = ""

        for arg in self.entities:
            if arg.array:
                continue

            if arg.direction == "input":
                if arg.optional:
                    result += f"    if ({arg.name}) {{\n"

                    if arg.type == "struct":
                        result += f"        args->{arg.name} = *{arg.name};\n"
                    elif arg.type == "enum" or arg.type == "int":
                        result += f"        args->{arg.name} = {arg.name};\n"
                    else:
                        result += f"        args->{arg.name}_id = {arg.type_name}_GetID( {arg.name} );\n"

                    result += f"        args->{arg.name}_set = true;\n" \
                               "    }\n" \
                               "    else\n" \
                              f"        args->{arg.name}_set = false;\n"
                else:
                    if arg.type == "struct":
                        result += f"    args->{arg.name} = *{arg.name};\n"
                    elif arg.type == "enum" or arg.type == "int":
                        result += f"    args->{arg.name} = {arg.name};\n"
                    else:
                        result += f"    args->{arg.name}_id = {arg.type_name}_GetID( {arg.name} );\n"

        for arg in self.entities:
            if not arg.array:
                continue

            if arg.direction == "input":
                if arg.optional:
                    result += f"  if ({arg.name}) {{\n"

                result +=  "    direct_memcpy( (char*) (args + 1)" \
                          f"{arg.offset(self, False, False)}, {arg.name}, {arg.size(False)} );\n"

                if arg.optional:
                    result += f"    args->{arg.name}_set = true;\n" \
                               "  }\n" \
                               "  else {\n" \
                              f"    args->{arg.name}_set = false;\n" \
                              f"    {arg.name} = 0;\n" \
                               "  }\n"

        return result

    def ArgumentsOutputAssignments(self):
        result = ""

        for arg in self.entities:
            if arg.array:
                continue

            if arg.direction == "output":
                if arg.type == "struct" or arg.type == "enum" or arg.type == "int":
                    if arg.optional:
                        result += f"    if (ret_{arg.name})\n" \
                                  f"        *ret_{arg.name} = return_args->{arg.name};\n"
                    else:
                        result += f"    *ret_{arg.name} = return_args->{arg.name};\n"

        for arg in self.entities:
            if not arg.array:
                continue

            if arg.direction == "output":
                result += f"    direct_memcpy( ret_{arg.name}, (char*) (return_args + 1)" \
                          f"{arg.offset(self, False, True)}, {arg.sizeReturn(self)} );\n"

        return result

    def ArgumentsAssertions(self):
        result = ""

        for arg in self.entities:
            if (arg.type == "struct" or arg.type == "object") and not arg.optional :
                result += "    D_ASSERT( " + (arg.name if arg.direction == "input" else f"ret_{arg.name}") + " != NULL );\n"

        return result

    def ArgumentsOutputObjectDecl(self):
        result = ""

        for arg in self.entities:
            if arg.direction == "output" and arg.type == "object":
                result += f"    {arg.type_name} *{arg.name} = NULL;\n"

        return result

    def ArgumentsOutputTmpDecl(self):
        result = ""
        second = False

        for arg in self.entities:
            if arg.direction == "output" and arg.array:
                if second:
                    result += f"    {arg.type_name}  tmp_{arg.name}[{arg.max}];\n"
                else:
                    second = True

        return result

    def ArgumentsInputObjectDecl(self):
        result = ""

        for arg in self.entities:
            if arg.direction == "input" and arg.type == "object":
                result += f"    {arg.type_name} *{arg.name} = NULL;\n"

        return result

    def ArgumentsOutputObjectCatch(self):
        result = ""

        for arg in self.entities:
            if arg.direction == "output" and arg.type == "object":
                result += f"    ret = (DFBResult) {arg.type_name}_Catch" \
                          f"( core_dfb, return_args->{arg.name}_ptr, &{arg.name} );\n" \
                           "    if (ret) {\n" \
                           "         D_DERROR( ret, \"%s: " \
                          f"Catching {arg.name} by ID %u failed!\\n\", __FUNCTION__, return_args->{arg.name}_id );\n" \
                           "         goto out;\n" \
                           "    }\n" \
                           "\n" \
                           "    *" + (arg.name if arg.direction == "input" else f"ret_{arg.name}") + f" = {arg.name};\n" \
                           "\n"

        return result

    def ArgumentsOutputObjectThrow(self):
        result = ""

        for arg in self.entities:
            if arg.type == "object" and arg.direction == "output":
                result += f"                {arg.type_name}_Throw" \
                          f"( {arg.name}, caller, &return_args->{arg.name}_id );\n" \
                          f"                return_args->{arg.name}_ptr = (void*) {arg.name};\n"

        return result

    def ArgumentsOutputTmpReturn(self):
        result = ""
        second_output_array = False

        for arg in self.entities:
            if arg.direction == "output" and arg.array:
                if second_output_array:
                    result += f"                direct_memcpy( (char*) ((char*)(return_args + 1)" \
                              f"{arg.offset(self, True, True)}), tmp_{arg.name}, return_args->{arg.name}_size );\n"
                else:
                    second_output_array = True

        return result

    def ArgumentsInputObjectLookup(self):
        result = ""

        for arg in self.entities:
            if arg.direction == "input" and arg.type == "object":
                if arg.optional:
                    result += f"            if (args->{arg.name}_set) {{\n" \
                              f"                ret = (DFBResult) {arg.type_name}_Lookup" \
                              f"( core_dfb, args->{arg.name}_id, caller, &{arg.name} );\n" \
                               "                if (ret) {\n" \
                              f"                     D_DERROR( ret, \"%s({self.name}): " \
                              f"Looking up {arg.name} by ID %u failed!\\n\", __FUNCTION__, args->{arg.name}_id );\n" + \
                              ("                     return_args->result = ret;\n" if self.sync else "") + \
                              ("                     D_BREAK( \"could not lookup object\" );\n" \
                               if config.dispatch_error_abort  else "") + \
                               "                     return ret;\n" \
                               "                }\n"
                else:
                    result += f"            ret = (DFBResult) {arg.type_name}_Lookup" \
                              f"( core_dfb, args->{arg.name}_id, caller, &{arg.name} );\n" \
                               "            if (ret) {\n" \
                              f"                 D_DERROR( ret, \"%s({self.name}): " \
                              f"Looking up {arg.name} by ID %u failed!\\n\", __FUNCTION__, args->{arg.name}_id );\n" + \
                              ("                 return_args->result = ret;\n" if self.sync else "") + \
                              ("                 D_BREAK( \"could not lookup object\" );\n" \
                               if config.dispatch_error_abort  else "") + \
                               "                 return ret;\n" \

                result += "            }\n" \
                          "\n"

        return result

    def ArgumentsInputDebug(self):
        result = ""

        for arg in self.entities:
            if arg.array:
                continue

            if arg.optional:
                result += f"  if (args->{arg.name}_set)\n"

            if arg.type == "struct":
                result += f"         ;    // TODO: {arg.type_name}_debug args->{arg.name};\n"
            else:
                result += f"            D_DEBUG_AT( DirectFB_{face.object}, \"  -> {arg.name} = "
                if arg.type == "object":
                    result += f"%u\\n\", args->{arg.name}_id );\n"
                else:
                    result += f"%{arg.formatCharacter()}\\n\", {arg.typeCast()}args->{arg.name} );\n"

        return result

    def ArgumentsInputObjectUnref(self):
        result = ""

        for arg in self.entities:
            if arg.direction == "input" and arg.type == "object":
               result += f"            if ({arg.name})\n" \
                         f"                {arg.type_name}_Unref( {arg.name} );\n" \
                          "\n"

        return result

    def ArgumentsNames(self):
        result = ""

        for index, arg in enumerate(self.entities):
            last = index == len(self.entities) - 1

            result += arg.name if arg.direction == "input" else "ret_" + arg.name

            if not last:
                result += ", "

        return result

    def ArgumentsSize(self, output):
        result = "sizeof("

        if output:
            result += f"{face.object}{self.name}Return)"
        else:
            result += f"{face.object}{self.name})"

        for arg in self.entities:
            if not arg.array:
                continue

            if output:
                if arg.direction == "output":
                    result += f" + {arg.sizeMax()}"
            else:
                if arg.direction == "input":
                    result += f" + {arg.size(False)}"

        return result

    def ArgumentsSizeReturn(self):
        result = f"sizeof({face.object}{self.name}Return)"

        for arg in self.entities:
            if not arg.array:
                continue

            if arg.direction == "output":
                result += f" + {arg.sizeReturn(self)}"

        return result

class Arg(Entity):
    array = False
    optional = False

    def SetProperty(self, name, value):
        if name == "name":
            self.name = value
        elif name == "direction":
            self.direction = value
        elif name == "type":
            self.type = value
        elif name == "typename":
            self.type_name = value
        elif name == "optional":
            self.optional = value == "yes"
        elif name == "count":
            self.array = True
            self.count = value
        elif name == "max":
            self.max = value

    def formatCharacter(self):
        if self.type == "int":
            if self.type_name == "u32":
                return "u"
            elif self.type_name == "s32":
                return "d"
            elif self.type_name == "u64":
                return "llu"
            elif self.type_name == "s64":
                return "lld"
            else:
                return "p"
        else:
            return "x"

    def typeCast(self):
        if self.type == "int":
            if self.type_name == "u64":
                return "(unsigned long long) "
            elif self.type_name == "s64":
                return "(long long) "

        return ""

    def size(self, use_args):
        if use_args:
            return f"args->{self.count} * sizeof({self.type_name})"
        else:
            return f"{self.count} * sizeof({self.type_name})"

    def sizeMax(self):
        return f"{self.max} * sizeof({self.type_name})"

    def sizeReturn(self, method):
        if self.array:
            for arg in method.entities:
                if arg.direction == "input":
                    return f"args->{self.count} * sizeof({self.type_name})"
                else:
                    return f"return_args->{self.count} * sizeof({self.type_name})"

    def offset(self, method, use_args, output):
        result = ""

        for arg in method.entities:
            if not arg.array:
                continue

            if arg == self:
                break

            if output:
                result += f" + return_args->{arg.size(False)}"
            else:
                result += f" + {arg.size(use_args)}"

        return result

# --------------------------------------------------------------------------------------------------------------------------

face = Interface()

def GetEntities(position, out_vector):
    with open(config.filename, 'r') as file:
        file.seek(position)

        content = file.read()

        index = 0
        level = 0
        comment = False
        quote = False
        name = ""
        names = {}

        for char in content:
            index += 1

            if comment:
                if char == '\n':
                    comment = False
            elif quote:
                if char == '"':
                    quote = False
                else:
                    name += char
            else:
                if char == '"':
                    quote = True
                elif char == '#':
                    comment = True
                elif char in ('.', '-', '_', *'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'):
                    name += char
                else:
                    if name:
                        if level in names and names[level]:
                            if level == 1:
                                entity.SetProperty(names[level], name)
                                name = ""

                        names[level] = name
                        name = ""

                    if char == '{':
                        if level == 0:
                            if names[level] == "interface":
                                entity = face
                                entity.position = position + index
                            elif names[level] == "method":
                                entity = Method()
                                entity.position = position + index
                            else:
                                entity = Arg()
                                entity.position = position + index

                        names[level] = ""
                        level += 1

                    elif char == '}':
                        level -= 1

                        if level == 0:
                            GetEntities(entity.position, entity.entities)
                            out_vector.append(entity)

                        if level < 0:
                            break

def GenerateHeader():
    with open((config.output_dir + "/" if config.output_dir else "") + face.object + ".h", "w") as file:
        file.write( "/*\n"
                    " * This file was automatically generated by fluxcomp; DO NOT EDIT!\n"
                    " */\n"
                   f"{license}"
                    "\n")

        # Start of header guard and includes

        file.write(f"#ifndef ___{face.object}__H___\n"
                   f"#define ___{face.object}__H___\n"
                    "\n"
                   f"#include <{config.include_prefix}/{face.object}_includes.h>\n"
                    "\n"
                    "/***************************************************************************************************\n"
                   f" * {face.object}\n"
                    " */\n"
                    "\n"
                    "#ifdef __cplusplus\n"
                    "#include <core/Interface.h>\n"
                    "\n"
                    "extern \"C\" {\n"
                    "#endif\n"
                    "\n"
                    "\n")

        # C Wrappers

        for method in face.entities:
            if isinstance(method, Method):
                file.write(f"DFBResult {face.object}_{method.name}(\n"
                           f"                    {face.object:<40}  *obj" +
                           (",\n" if method.entities else "\n") +
                           f"{method.ArgumentsAsParamDecl()});\n"
                            "\n")

        file.write( "\n"
                   f"void {face.object}_Init_Dispatch(\n"
                    "                    CoreDFB              *core,\n"
                   f"                    {face.dispatch:<20} *obj,\n"
                    "                    FusionCall           *call\n"
                    ");\n"
                    "\n"
                   f"void  {face.object}_Deinit_Dispatch(\n"
                    "                    FusionCall           *call\n"
                    ");\n"
                    "\n"
                    "\n"
                    "#ifdef __cplusplus\n"
                    "}\n"
                    "#endif\n"
                    "\n"
                    "\n"
                    "\n"
                    "\n")

        # Method IDs

        file.write( "/*\n"
                   f" * {face.object} Calls\n"
                    " */\n"
                    "typedef enum {\n")

        index = 1
        for method in face.entities:
            if isinstance(method, Method):
                file.write(f"    _{face.object}_{method.name} = {index},\n")
                index += 1

        file.write(f"}} {face.object}Call;\n"
                    "\n")

        # Method Argument Structures

        for method in face.entities:
            if isinstance(method, Method):
                file.write( "/*\n"
                           f" * {face.object}_{method.name}\n"
                            " */\n"
                            "typedef struct {\n"
                           f"{method.ArgumentsAsMemberDecl()}"
                           f"}} {face.object}{method.name};\n"
                            "\n"
                            "typedef struct {\n"
                           f"{method.ArgumentsOutputAsMemberDecl()}"
                           f"}} {face.object}{method.name}Return;\n"
                            "\n"
                            "\n")

        # Real Interface

        for method in face.entities:
            if isinstance(method, Method):
                file.write(f"DFBResult {face.name}_Real__{method.name}( {face.dispatch} *obj" +
                           (",\n" if method.ArgumentsAsParamDecl() else "\n") +
                           f"{method.ArgumentsAsParamDecl()} );\n"
                            "\n")

        # Requestor Interface

        for method in face.entities:
            if isinstance(method, Method):
                file.write(f"DFBResult {face.name}_Requestor__{method.name}( {face.object} *obj" +
                           (",\n" if method.ArgumentsAsParamDecl() else "\n") +
                           f"{method.ArgumentsAsParamDecl()} );\n"
                           "\n")

        # Dispatch Function

        file.write( "\n"
                   f"DFBResult {face.object}Dispatch__Dispatch( {face.dispatch} *obj,\n"
                    "                    FusionID      caller,\n"
                    "                    int           method,\n"
                    "                    void         *ptr,\n"
                    "                    unsigned int  length,\n"
                    "                    void         *ret_ptr,\n"
                    "                    unsigned int  ret_size,\n"
                    "                    unsigned int *ret_length );\n"
                    "\n")

        # End of header guard

        file.write( "\n"
                    "#endif\n")

def GenerateSource():
    direct = True
    if face.object != face.dispatch:
        direct = False

    with open((config.output_dir + "/" if config.output_dir else "") + face.object + ".c", "w") as file:
        file.write( "/*\n"
                    " * This file was automatically generated by fluxcomp; DO NOT EDIT!\n"
                    " */\n"
                   f"{license}"
                    "\n")

        # includes

        file.write( "#include <config.h>\n"
                    "\n"
                   f"#include \"{face.object}.h\"\n"
                    "\n"
                    "#include <directfb_util.h>\n"
                    "\n"
                    "#include <direct/debug.h>\n"
                    "#include <direct/mem.h>\n"
                    "#include <direct/memcpy.h>\n"
                    "#include <direct/messages.h>\n"
                    "\n"
                    "#include <fusion/conf.h>\n"
                    "\n"
                    "#include <core/core.h>\n"
                    "\n"
                    "#include <core/CoreDFB_CallMode.h>\n"
                    "\n"
                   f"D_DEBUG_DOMAIN( DirectFB_{face.object}, \"DirectFB/{face.object}\", \"DirectFB {face.object}\" );\n"
                    "\n"
                    "/**************************************************************************************************/\n"
                    "\n")

        # C Wrappers

        for method in face.entities:
            if isinstance(method, Method):
                file.write( "DFBResult\n"
                           f"{face.object}_{method.name}(\n"
                           f"                    {face.object:<40}  *obj" +
                           (",\n" if method.entities else "\n") +
                           f"{method.ArgumentsAsParamDecl()}\n"
                            ")\n"
                            "{\n"
                            "    DFBResult ret;\n"
                            "\n"
                            "    switch (CoreDFB_CallMode( core_dfb )) {\n"
                            "        case COREDFB_CALL_DIRECT:")

                if direct:
                    file.write( "{\n"
                                "            Core_PushCalling();\n"
                               f"            ret = {face.name}_Real__{method.name}( obj" +
                               (", " if method.ArgumentsAsParamDecl() else "") + f"{method.ArgumentsNames()} );\n"
                                "            Core_PopCalling();\n"
                                "\n"
                                "            return ret;\n"
                                "        }\n")

                file.write( "\n        case COREDFB_CALL_INDIRECT: {\n"
                            "            Core_PushCalling();\n"
                           f"            ret = {face.name}_Requestor__{method.name}( obj" +
                           (", " if method.ArgumentsAsParamDecl() else "") + f"{method.ArgumentsNames()} );\n"
                            "            Core_PopCalling();\n"
                            "\n"
                            "            return ret;\n"
                            "        }\n"
                            "        case COREDFB_CALL_DENY:\n"
                            "            return DFB_DEAD;\n"
                            "    }\n"
                            "\n"
                            "    return DFB_UNIMPLEMENTED;\n"
                            "}\n"
                            "\n")

        file.write( "/**************************************************************************************************/\n"
                    "\n"
                    "static FusionCallHandlerResult\n"
                   f"{face.object}_Dispatch( int           caller,   /* fusion id of the caller */\n"
                    "                     int           call_arg, /* optional call parameter */\n"
                    "                     void         *ptr, /* optional call parameter */\n"
                    "                     unsigned int  length,\n"
                    "                     void         *ctx,      /* optional handler context */\n"
                    "                     unsigned int  serial,\n"
                    "                     void         *ret_ptr,\n"
                    "                     unsigned int  ret_size,\n"
                    "                     unsigned int *ret_length )\n"
                    "{\n"
                   f"    {face.dispatch} *obj = ({face.dispatch}*) ctx;\n"
                   f"    {face.object}Dispatch__Dispatch"
                    "( obj, caller, call_arg, ptr, length, ret_ptr, ret_size, ret_length );\n"
                    "\n"
                    "    return FCHR_RETURN;\n"
                    "}\n"
                    "\n"
                   f"void {face.object}_Init_Dispatch(\n"
                    "                    CoreDFB              *core,\n"
                   f"                    {face.dispatch:<20} *obj,\n"
                    "                    FusionCall           *call\n"
                    ")\n"
                    "{\n"
                   f"    fusion_call_init3( call, {face.object}_Dispatch, obj, core->world );\n"
                    "}\n"
                    "\n"
                   f"void  {face.object}_Deinit_Dispatch(\n"
                    "                    FusionCall           *call\n"
                    ")\n"
                    "{\n"
                    "     fusion_call_destroy( call );\n"
                    "}\n"
                    "\n"
                    "/**************************************************************************************************/\n"
                    "\n")

        # Requestor Methods

        file.write( "static __inline__ void *args_alloc( void *static_buffer, size_t size )\n"
                    "{\n"
                    "    void *buffer = static_buffer;\n"
                    "\n"
                   f"    if (size > {config.static_args_bytes}) {{\n"
                    "        buffer = D_MALLOC( size );\n"
                    "        if (!buffer)\n"
                    "            return NULL;\n"
                    "    }\n"
                    "\n"
                    "    return buffer;\n"
                    "}\n"
                    "\n"
                    "static __inline__ void args_free( void *static_buffer, void *buffer )\n"
                    "{\n"
                    "    if (buffer != static_buffer)\n"
                    "        D_FREE( buffer );\n"
                    "}\n"
                    "\n")

        for method in face.entities:
            if isinstance(method, Method):
                file.write( "\n"
                            "DFBResult\n"
                           f"{face.name}_Requestor__{method.name}( {face.object} *obj" +
                           (",\n" if method.ArgumentsAsParamDecl() else "\n") +
                           f"{method.ArgumentsAsParamDecl()}\n"
                            ")\n"
                            "{\n"
                            "    DFBResult           ret = DFB_OK;\n"
                           f"{method.ArgumentsOutputObjectDecl()}"
                           f"    char        args_static[{config.static_args_bytes}];\n")

                if method.sync:
                    file.write(f"    char        return_args_static[{config.static_args_bytes}];\n")

                file.write(f"    {face.object}{method.name}       *args = ({face.object}{method.name}*) args_alloc"
                           f"( args_static, {method.ArgumentsSize(False)} );\n")

                if method.sync:
                    file.write(f"    {face.object}{method.name}Return *return_args;\n")

                file.write( "\n"
                            "    if (!args)\n"
                            "        return (DFBResult) D_OOM();\n"
                            "\n")

                if method.sync:
                    file.write(f"    return_args = ({face.object}{method.name}Return*) args_alloc"
                               f"( return_args_static, {method.ArgumentsSize(True)} );\n"
                                "\n")

                if method.sync:
                    file.write( "    if (!return_args) {\n"
                                "        args_free( args_static, args );\n"
                                "        return (DFBResult) D_OOM();\n"
                                "    }\n"
                                "\n")

                file.write(f"    D_DEBUG_AT( DirectFB_{face.object}, \"{face.name}_Requestor::%s()\\n\", __FUNCTION__ );\n"
                            "\n"
                           f"{method.ArgumentsAssertions()}"
                            "\n"
                           f"{method.ArgumentsInputAssignments()}"
                            "\n"
                           f"    ret = (DFBResult) {face.object}_Call")

                if method.sync:
                    file.write( "( obj, FCEF_NONE"
                               f", _{face.object}_{method.name}, args, {method.ArgumentsSize(False)}"
                               f", return_args, {method.ArgumentsSize(True)}, NULL );\n")
                else:
                    file.write("( obj, (FusionCallExecFlags)(FCEF_ONEWAY" + (" | FCEF_QUEUE" if method.queue else "") + ")"
                               f", _{face.object}_{method.name}, args, {method.ArgumentsSize(False)}"
                                ", NULL, 0, NULL );\n")

                file.write( "    if (ret) {\n"
                            "        D_DERROR( ret, \"%s: "
                           f"{face.object}_Call( {face.object}_{method.name} ) failed!\\n\", __FUNCTION__ );\n"
                            "        goto out;\n"
                            "    }\n"
                            "\n")

                if method.sync:
                    file.write( "    if (return_args->result) {\n"
                                "        /*D_DERROR( return_args->result, \"%s: "
                               f"{face.object}_{method.name} failed!\\n\", __FUNCTION__ );*/\n"
                                "        ret = return_args->result;\n"
                                "        goto out;\n"
                                "    }\n"
                                "\n")

                file.write(f"{method.ArgumentsOutputAssignments()}"
                            "\n"
                           f"{method.ArgumentsOutputObjectCatch()}"
                            "\n"
                            "out:\n")

                if method.sync:
                    file.write( "    args_free( return_args_static, return_args );\n")

                file.write( "    args_free( args_static, args );\n"
                            "    return ret;\n"
                            "}\n"
                            "\n")

        # Dispatch Object

        file.write( "/**************************************************************************************************/\n"
                    "\n"
                    "static DFBResult\n"
                   f"__{face.object}Dispatch__Dispatch( {face.dispatch} *obj,\n"
                    "                                FusionID      caller,\n"
                    "                                int           method,\n"
                    "                                void         *ptr,\n"
                    "                                unsigned int  length,\n"
                    "                                void         *ret_ptr,\n"
                    "                                unsigned int  ret_size,\n"
                    "                                unsigned int *ret_length )\n"
                    "{\n"
                    "    D_UNUSED\n"
                    "    DFBResult ret;\n"
                    "\n"
                    "\n"
                    "    switch (method) {\n")

        # Dispatch Methods

        for method in face.entities:
            if isinstance(method, Method):
                file.write(f"        case _{face.object}_{method.name}: {{\n"
                           f"{method.ArgumentsInputObjectDecl()}"
                           f"{method.ArgumentsOutputObjectDecl()}")

                if method.sync:
                    file.write(f"{method.ArgumentsOutputTmpDecl()}")

                file.write( "            D_UNUSED\n"
                           f"            {face.object}{method.name}       *args        = "
                           f"({face.object}{method.name} *) ptr;\n")

                if method.sync:
                    file.write(f"            {face.object}{method.name}Return *return_args = "
                               f"({face.object}{method.name}Return *) ret_ptr;\n")

                file.write( "\n"
                           f"            D_DEBUG_AT( DirectFB_{face.object}, \"=-> {face.object}_{method.name}\\n\" );\n"
                            "\n")

                if not method.sync:
                    file.write(f"{method.ArgumentsInputDebug()}"
                                "\n")

                file.write(f"{method.ArgumentsInputObjectLookup()}")

                if not method.sync:
                    file.write(f"            {face.name}_Real__{method.name}( obj" +
                               (", " if method.ArgumentsAsParamDecl() else "") + f"{method.ArgumentsAsMemberParams()} );\n")
                else:
                    file.write(f"            return_args->result = {face.name}_Real__{method.name}( obj" +
                               (", " if method.ArgumentsAsParamDecl() else "") + f"{method.ArgumentsAsMemberParams()} );\n"
                                "            if (return_args->result == DFB_OK) {\n"
                               f"{method.ArgumentsOutputObjectThrow()}"
                               f"{method.ArgumentsOutputTmpReturn()}"
                                "            }\n"
                                "\n"
                               f"            *ret_length = {method.ArgumentsSizeReturn()};\n")

                file.write( "\n"
                           f"{method.ArgumentsInputObjectUnref()}"
                            "            return DFB_OK;\n"
                            "        }\n"
                            "\n")

        file.write( "    }\n"
                    "\n"
                    "    return DFB_NOSUCHMETHOD;\n"
                    "}\n")

        file.write( "/**************************************************************************************************/\n"
                    "\n"
                    "DFBResult\n"
                   f"{face.object}Dispatch__Dispatch( {face.dispatch} *obj,\n"
                    "                                FusionID      caller,\n"
                    "                                int           method,\n"
                    "                                void         *ptr,\n"
                    "                                unsigned int  length,\n"
                    "                                void         *ret_ptr,\n"
                    "                                unsigned int  ret_size,\n"
                    "                                unsigned int *ret_length )\n"
                    "{\n"
                    "    DFBResult ret = DFB_OK;\n"
                    "\n"
                   f"    D_DEBUG_AT( DirectFB_{face.object}, \"{face.object}Dispatch::%s( %p )\\n\", "
                    "__FUNCTION__, obj );\n")

        if config.identity:
            file.write( "\n"
                        "    Core_PushIdentity( caller );\n")

        file.write( "\n"
                   f"    ret = __{face.object}Dispatch__Dispatch"
                    "( obj, caller, method, ptr, length, ret_ptr, ret_size, ret_length );\n")

        if config.identity:
            file.write( "\n"
                        "    Core_PopIdentity();\n")

        file.write( "\n"
                    "    return ret;\n"
                    "}\n")

# --------------------------------------------------------------------------------------------------------------------------

config = parser.parse_args()

GetEntities(0, face.entities)

GenerateHeader()

GenerateSource()
