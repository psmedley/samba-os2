#!/usr/bin/env python

#
# Unix SMB/CIFS implementation.
#
# HRESULT Error definitions
#
# Copyright (C) Noel Power <noel.power@suse.com> 2014
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#


import sys, os.path, io, string

# parsed error data
Errors = []

# error data model
class ErrorDef:

    def __init__(self):
        self.err_code = ""
        self.err_define = None
        self.err_string = ""
        self.isWinError = False
        self.linenum = ""

def escapeString( input ):
    output = input.replace('"','\\"')
    output = output.replace("\\<","\\\\<")
    output = output.replace('\t',"")
    return output

def parseErrorDescriptions( input_file, isWinError ):
    # read in the data
    fileContents = open(input_file,"r")
    count = 0;
    for line in fileContents:
        content = line.strip().split(None,1)
        # start new error definition ?
        if line.startswith("0x"):
            newError = ErrorDef()
            newError.err_code = content[0]
            # escape the usual suspects
            if len(content) > 1:
                newError.err_string = escapeString(content[1])
            newError.linenum = count
            newError.isWinError = isWinError
            Errors.append(newError)
        else:
            if len(Errors) == 0:
                print "Error parsing file as line %d"%count
                sys.exit()
            err = Errors[-1]
            if err.err_define == None:
                err.err_define = "HRES_" + content[0]
        else:
            if len(content) > 0:
                desc =  escapeString(line.strip())
                if len(desc):
                    if err.err_string == "":
                        err.err_string = desc
                    else:
                        err.err_string = err.err_string + " " + desc
        count = count + 1
    fileContents.close()
    print "parsed %d lines generated %d error definitions"%(count,len(Errors))

def write_license(out_file):
    out_file.write("/*\n")
    out_file.write(" * Unix SMB/CIFS implementation.\n")
    out_file.write(" *\n")
    out_file.write(" * HRESULT Error definitions\n")
    out_file.write(" *\n")
    out_file.write(" * Copyright (C) Noel Power <noel.power@suse.com> 2014\n")
    out_file.write(" *\n")
    out_file.write(" * This program is free software; you can redistribute it and/or modify\n")
    out_file.write(" * it under the terms of the GNU General Public License as published by\n")
    out_file.write(" * the Free Software Foundation; either version 3 of the License, or\n")
    out_file.write(" * (at your option) any later version.\n")
    out_file.write(" *\n")
    out_file.write(" * This program is distributed in the hope that it will be useful,\n")
    out_file.write(" * but WITHOUT ANY WARRANTY; without even the implied warranty of\n")
    out_file.write(" * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n")
    out_file.write(" * GNU General Public License for more details.\n")
    out_file.write(" *\n")
    out_file.write(" * You should have received a copy of the GNU General Public License\n")
    out_file.write(" * along with this program.  If not, see <http://www.gnu.org/licenses/>.\n")
    out_file.write(" */\n")
    out_file.write("\n")

def generateHeaderFile(out_file):
    write_license(out_file)
    out_file.write("#ifndef _HRESULT_H_\n")
    out_file.write("#define _HRESULT_H_\n\n")
    macro_magic = "#if defined(HAVE_IMMEDIATE_STRUCTURES)\n"
    macro_magic += "typedef struct {uint32_t h;} HRESULT;\n"
    macro_magic += "#define HRES_ERROR(x) ((HRESULT) { x })\n"
    macro_magic += "#define HRES_ERROR_V(x) ((x).h)\n"
    macro_magic += "#else\n"
    macro_magic += "typedef uint32_t HRESULT;\n"
    macro_magic += "#define HRES_ERROR(x) (x)\n"
    macro_magic += "#define HRES_ERROR_V(x) (x)\n"
    macro_magic += "#endif\n"
    macro_magic += "\n"
    macro_magic += "#define HRES_IS_OK(x) (HRES_ERROR_V(x) == 0)\n"
    macro_magic += "#define HRES_IS_EQUAL(x,y) (HRES_ERROR_V(x) == HRES_ERROR_V(y))\n"

    out_file.write(macro_magic)
    out_file.write("\n\n")
    out_file.write("/*\n")
    out_file.write(" * The following error codes are autogenerated from [MS-ERREF]\n")
    out_file.write(" * see http://msdn.microsoft.com/en-us/library/cc704587.aspx\n")
    out_file.write(" */\n")
    out_file.write("\n")

    for err in Errors:
        line = "#define {0:49} HRES_ERROR({1})\n".format(err.err_define ,err.err_code)
        out_file.write(line)
    out_file.write("\nconst char *hresult_errstr_const(HRESULT err_code);\n")
    out_file.write("\n#define FACILITY_WIN32 0x0007\n")
    out_file.write("#define WIN32_FROM_HRESULT(x) (HRES_ERROR_V(x) == 0 ? HRES_ERROR_V(x) : ~((FACILITY_WIN32 << 16) | 0x80000000) & HRES_ERROR_V(x))\n")
    out_file.write("#define HRESULT_IS_LIKELY_WERR(x) ((HRES_ERROR_V(x) & 0xFFFF0000) == 0x80070000)\n")
    out_file.write("\n\n\n#endif /*_HRESULT_H_*/")


def generateSourceFile(out_file):
    write_license(out_file)
    out_file.write("#include \"includes.h\"\n")
    out_file.write("#include \"hresult.h\"\n")
    out_file.write("/*\n")
    out_file.write(" * The following error codes and descriptions are autogenerated from [MS-ERREF]\n")
    out_file.write(" * see http://msdn.microsoft.com/en-us/library/cc704587.aspx\n")
    out_file.write(" */\n")
    out_file.write("\n")
    out_file.write("static const struct {\n")
    out_file.write("	HRESULT error_code;\n")
    out_file.write("	const char *error_str;\n")
    out_file.write("} hresult_errs[] = {\n")

    for err in Errors:
        out_file.write("	{\n")
        if err.isWinError:
            out_file.write("		HRESULT_FROM_WIN32(%s),\n"%err.err_define)
        else:
            out_file.write("		%s,\n"%err.err_define)
        out_file.write("		\"%s\"\n"%err.err_string)
        out_file.write("	},\n")
    out_file.write("};\n")
    out_file.write("\n")
    out_file.write("const char *hresult_errstr_const(HRESULT err_code)\n")
    out_file.write("{\n");
    out_file.write("	const char *result = NULL;\n")
    out_file.write("	int i;\n")
    out_file.write("	for (i = 0; i < ARRAY_SIZE(hresult_errs); ++i) {\n")
    out_file.write("		if (HRES_IS_EQUAL(err_code, hresult_errs[i].error_code)) {\n")
    out_file.write("			result = hresult_errs[i].error_str;\n")
    out_file.write("			break;\n")
    out_file.write("		}\n")
    out_file.write("	}\n")
    out_file.write("	/* convert & check win32 error space? */\n")
    out_file.write("	if (result == NULL && HRESULT_IS_LIKELY_WERR(err_code)) {\n")
    out_file.write("		WERROR wErr = W_ERROR(WIN32_FROM_HRESULT(err_code));\n")
    out_file.write("		result = get_friendly_werror_msg(wErr);\n")
    out_file.write("	}\n")
    out_file.write("	return result;\n")
    out_file.write("};\n")

# Very simple script to generate files hresult.c & hresult.h
# The script simply takes a text file as input, format of input file is
# very simple and is just the content of a html table ( such as that found
# in http://msdn.microsoft.com/en-us/library/cc704587.aspx ) copied and
# pasted into a text file

def main ():
    input_file1 = None;
    filename = "hresult"
    headerfile_name = filename + ".h"
    sourcefile_name = filename + ".c"
    if len(sys.argv) > 1:
        input_file1 =  sys.argv[1]
    else:
        print "usage: %s winerrorfile"%(sys.argv[0])
        sys.exit()

    parseErrorDescriptions(input_file1, False)
    out_file = open(headerfile_name,"w")
    generateHeaderFile(out_file)
    out_file.close()
    out_file = open(sourcefile_name,"w")
    generateSourceFile(out_file)

if __name__ == '__main__':

    main()
