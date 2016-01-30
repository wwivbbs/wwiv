import sys
import os
import stat
import re


#Helper Functions
#==========================================================================

def parseEnumContents(enumContents, nameSpace, debugString):
    enumPattern = re.compile(r"(CRYPT_[\w_]*)\s*(=.*?)?(\Z|,|(?=/))", re.DOTALL)     # %1 [= %2][,]
    commentPattern = re.compile(r"\s*/\*(.*)\*/")                                    # /* %1 */
    counter = 0     #Keep track of the implicit enum value
    enumTuples = [] #Build a list of (key, val, comment) tuples, one for each enum value

    #Find the next "enum value"
    enumMatch = enumPattern.search(enumContents)
    while enumMatch:

        #Extract the key and RHS value
        rawKey, rawVal = enumMatch.groups()[:-1]

        key = rawKey.rstrip()
        if not key.startswith("CRYPT_"):
            raise "enum'd value doesn't start with CRYPT_ "+key
        key = key.replace("CRYPT_", "")

        #Evaluate the RHS (if it exists) to get the value
        if rawVal:
            rhs = rawVal[1:].strip().replace("CRYPT_", "")
            val = eval(rhs, nameSpace)
        #Otherwise the value is the implicit counter
        else:
            val = counter

        #Extract the comment
        commentMatch = commentPattern.match(enumContents, enumMatch.end())
        if commentMatch:
            rawComment = commentMatch.group(1)
            comment = rawComment.strip()
        else:
            comment = ""

        #Collect all the parsed values into a tuple
        enumTuple = (key, str(val), comment)

        #if comment:
        #    print debugString+ ":" + " Parsed Element %s = %s /* %s */" % enumTuple
        #else:
        #    print debugString+ ":" + " Parsed Element %s = %s" % enumTuple[:-1]
        #raw_input()

        nameSpace[key] = val    #Add new enum value into namespace
        counter = val + 1       #Increment implicit enum value

        #Accumulate the parsed (key, val, comment) tuples
        enumTuples.append(enumTuple)

        #Get next enum value
        enumMatch = enumPattern.search(enumContents, enumMatch.end())

    return enumTuples

#Parses a string of function parameters into a list of ParamStruct
class ParamStruct:
    def __init__(self):
        self.type = ""
        self.isPtr = 0
        self.isOut = 0
        self.category = ""
        self.name = ""
        self.rawIndex = 0 # The index of this param in the initial, raw cryptlib header
    def __str__(self):
        return str( (self.type, self.isPtr, self.category, self.name, self.rawIndex) )

def parseFunctionParams(functionParams):
    paramStructs = []
    functionParams = functionParams.replace("\n", "") #Make one big line
    functionParamsList = [e.strip() for e in functionParams.split(",")] #Break into a list of params
    index = 0
    for e in functionParamsList:
        pieces = e.split(" ")   #Separate each param into component pieces, ie ["C_IN", "CRYPT_ALGO_TYPE", "cryptAlgo"]
        p = ParamStruct()
        if len(pieces)==1 and pieces[0]=="void":
            continue
        elif len(pieces)==3:
            if pieces[0] == "C_OUT":
                p.isOut = 1
            if pieces[0] == "C_OUT_OPT":
                p.isOut = 1
            if pieces[1] == "C_STR":
                p.type = "char"
                p.name = pieces[2]
                p.isPtr = 1
            else:
                p.type = pieces[1]
                p.name = pieces[2]
                p.isPtr = 0
        elif len(pieces)==4:    #Ie ["C_OUT", "CRYPT_CONTEXT", "C_PTR", "cryptContext"]
            if pieces[0] == "C_OUT":
                p.isOut = 1
            if pieces[0] == "C_OUT_OPT":
                p.isOut = 1
            p.type = pieces[1]
            if pieces[2] == "C_PTR":
                p.isPtr = 1
            else:
                raise "Expecting C_PTR in parseFunctionParams"
            p.name = pieces[3]
        else:
            raise "parsing error in parseFunctionParams"
        if p.type in enumTypes:
            p.category = "enumType"
        elif p.type in intTypes:
            p.category = "intType"
        elif p.type in structTypes:
            p.category = "structType"
        elif p.type in rawTypes:
            p.category = "rawType"
        else:
            raise "unknown type error is parseFunctionParams"
        p.rawIndex = index
        index += 1
        paramStructs.append(p)
    #for p in paramStructs:
    #    print p.__dict__
    #raw_input()
    return paramStructs

#Takes a string containing a JNI function parameters prototype list, and a list of ParamStructs
#And injects names into the string and returns it
def expandFunctionPrototype(functionPrototype, newParamStructs):
    functionPrototype = functionPrototype.replace("\n", "") #Make one big line
    paramPrototypesList = [e.strip() for e in functionPrototype.split(",")] #Break into a list of params
    paramNamesList = ["env", "cryptClass"] + [e.name for e in newParamStructs]
    newFunctionParams = ""
    for (p1, p2) in zip(paramPrototypesList, paramNamesList):
        newFunctionParams += p1 + " " + p2 + ", "
    newFunctionParams = newFunctionParams[:-2]
    return newFunctionParams






#Main
#=========================================================================
#Execution starts here...
#=========================================================================
if len(sys.argv) != 4:
    print "cryptlibConverter.py <inFile> <outDir> <language>"
    sys.exit()

inFile = sys.argv[1]
outDir = sys.argv[2]
language = sys.argv[3]

if not os.path.exists(outDir):
    print "Making output directory..."
    os.mkdir(outDir)

if not language in ("java", "python", "net"):
    print "only java, python, and net are supported!"
    sys.exit()

if language == "java":
    #ENUM IDIOM
    #typedefEnumTemplate = "public static class %(typedefName)s\n{public int getValue(){return m_value;}private %(typedefName)s(int value){m_value = value;}int m_value;}"
    #typedefEnumElementTemplate = "public static final %(typedefName)s %(name)-NPADs = new %(typedefName)s(%(value)-VPADs);"
    #ENUMs as ints
    typedefEnumTemplate = "// %(typedefName)s"
    typedefEnumElementTemplate = "public static final int %(name)-NPADs = %(value)-VPADs;"

    typedefEnumElementTemplateComment = typedefEnumElementTemplate + " // %(comment)s"
    simpleEnumElementTemplate = "public static final int %(name)-NPADs = %(value)-VPADs;"
    simpleEnumElementTemplateComment = simpleEnumElementTemplate + " // %(comment)s"
    defineNPad = "40"
    defineVPad = "4"
    defineTemplate = simpleEnumElementTemplate
    defineTemplateComment = simpleEnumElementTemplateComment
    exceptionPrefix = """
package cryptlib;

public class CryptException extends Exception
{
    private int m_status;
    private String m_message;
    public CryptException(int status)
    {
        m_status = status;
        String prefix = Integer.toString(status) + ": ";

"""
    exceptionPostfix = """\
        m_message = prefix + "Unknown Exception ?!?!";
    }

    public int getStatus()
    {
        return m_status;
    }

    public String getMessage()
    {
        return m_message;
    }
};"""
    exceptionTemplate = """\
        if (m_status == crypt.%(name)s) {
            m_message = prefix + "%(comment)s";
            return; }
"""
    cryptQueryInfoString = """
package cryptlib;

public class CRYPT_QUERY_INFO
{
    public String algoName;
    public int blockSize;
    public int minKeySize;
    public int keySize;
    public int maxKeySize;

    public CRYPT_QUERY_INFO(String newAlgoName, int newBlockSize, int newMinKeySize, int newKeySize, int newMaxKeySize)
    {
        algoName = newAlgoName;
        blockSize = newBlockSize;
        minKeySize = newMinKeySize;
        keySize = newKeySize;
        maxKeySize = newMaxKeySize;
    }
};"""
    cryptObjectInfoString = """
package cryptlib;

public class CRYPT_OBJECT_INFO
{
    public int objectType;
    public int cryptAlgo;
    public int cryptMode;
    public int hashAlgo;
    public byte[] salt;

    public CRYPT_OBJECT_INFO(int newObjectType, int newCryptAlgo, int newCryptMode, int newHashAlgo, byte[] newSalt)
    {
        objectType = newObjectType;
        cryptAlgo = newCryptAlgo;
        cryptMode = newCryptMode;
        hashAlgo = newHashAlgo;
        salt = newSalt;
    }
};"""
    addFunctionWrappers = 1
    wholeFunctionDeclaration = None
    functionDeclaration = "public static native "
    returnIntDeclaration = "int "
    returnVoidDeclaration = "void "
    paramsPrefix = "("
    paramsPostfix = ") throws CryptException;"
    paramWhiteSpace = "\t\t\t\t\t\t"
    paramVoidPtrTemplate = "java.nio.ByteBuffer %(name)s,\n"
    addFunctionAlternate = 1
    paramVoidPtrAlternate = ("java.nio.ByteBuffer", "byte[]")
    paramCharPtrTemplate = "String %(name)s,\n"
    paramIntTemplate = "int %(name)s,\n"
    paramIntTypeTemplate = "int %(name)s, // %(type)s\n"
    #wrapperLengthTemplate = "%s.capacity(), "
    #wrapperStringLengthTemplate = "%s.length(), "
    wrapperLengthTemplate = "%(1)s == null ? 0 : %(1)s.capacity(), "
    wrapperStringLengthTemplate = "%(1)s == null ? 0 : %(1)s.getBytes().length, "
    wrapperStringReplace = ("java.nio.ByteBuffer", "String")
    wrapperStringTemplate = '%(1)s == null ? null : %(1)s.getBytes()'
    #ENUM IDIOM
    #paramEnumTypeTemplate = "%(type)s %(name)s,\n"
    #ENUMs as ints
    paramEnumTypeTemplate = "int %(name)s, // %(type)s\n"
    commentPrefix = "//"
    classPrefix = "package cryptlib;\n\nimport java.nio.*;\n\npublic class crypt\n{\n"
    classPostfix = "\n};"
    sFuncs = None
    sInts = None
elif language == "python":
    typedefEnumTemplate = ""
    #typedefEnumElementTemplate = "%(name)-NPADs = %(value)-VPADs"
    typedefEnumElementTemplate = """\

    v = Py_BuildValue("i", %(value)s);
    PyDict_SetItemString(moduleDict, "CRYPT_%(name)s", v);
    Py_DECREF(v);"""
    typedefEnumElementTemplateComment = typedefEnumElementTemplate + " /* %(comment)s */"
    simpleEnumElementTemplate = typedefEnumElementTemplate
    simpleEnumElementTemplateComment = typedefEnumElementTemplateComment
    defineNPad = "40"
    defineVPad = "4"
    defineTemplate = typedefEnumElementTemplate
    defineTemplateComment = typedefEnumElementTemplateComment
    exceptionPrefix = ""
    exceptionPostfix = ""
    exceptionTemplate = """\
    else if (status == CRYPT_%(name)s)
        o = Py_BuildValue("(is)", CRYPT_%(name)s, "%(comment)s");
"""
    addFunctionWrappers = 0
    wholeFunctionDeclaration = """\
static PyObject* python_crypt%s(PyObject* self, PyObject* args)
{
}
"""
    moduleFunctionEntry = "\t{ \"crypt%s\", python_crypt%s, METH_VARARGS }, "
    pyBeforeExceptions = r"""
#include <Python.h>
#include "../cryptlib.h"


static PyObject* cryptHandleClass;
static PyObject* cryptQueryInfoClass;
static PyObject* cryptObjectInfoClass;
static PyObject *CryptException;

static int getPointerWrite(PyObject* objPtr, unsigned char** bytesPtrPtr, int* lengthPtr)
{
    if (objPtr == Py_None)
    {
        *bytesPtrPtr = NULL;
        *lengthPtr = 0;
        return 1;
    }

    Py_ssize_t size = 0;

    /*See if it's an array object*/
    if (PyObject_AsWriteBuffer(objPtr, (void **)bytesPtrPtr, &size) == -1)
        return 0;
    *lengthPtr = size;
    return 1;
}

static int getPointerRead(PyObject* objPtr, unsigned char** bytesPtrPtr, int* lengthPtr)
{
    if (objPtr == Py_None)
    {
        *bytesPtrPtr = NULL;
        *lengthPtr = 0;
        return 1;
    }

    Py_ssize_t size = 0;

    /*See if it's an array object*/
    if (PyObject_AsWriteBuffer(objPtr, (void **)bytesPtrPtr, &size) == -1)
    {
        PyErr_Clear();
        /*See if it's a string object*/
        /*This returns the length excluding the NULL if it's a string,
          which is what we want*/
        if (PyObject_AsCharBuffer(objPtr, (const char **)bytesPtrPtr, &size) == -1)
            return 0;
    }
    *lengthPtr = size;
    return 1;
}

static int getPointerReadNoLength(PyObject* objPtr, unsigned char** bytesPtrPtr)
{
    int length;
    return getPointerRead(objPtr, bytesPtrPtr, &length);
}

static int getPointerWriteCheckIndices(PyObject* objPtr, unsigned char** bytesPtrPtr, int* lengthPtr)
{
    int checkLength = *lengthPtr;

    if (getPointerWrite(objPtr, bytesPtrPtr, lengthPtr) == 0)
        return 0;

    //If sequence is non-NULL and too short...
    if (*bytesPtrPtr && (*lengthPtr < checkLength))
    {
        PyErr_SetString(PyExc_IndexError, "A sequence passed to cryptlib was too small");
        return 0;
    }
    return 1;
}

static int getPointerReadString(PyObject* objPtr, char** charPtrPtr)
{
    Py_ssize_t length = 0;
    char* newPtr = NULL;

    if (objPtr == Py_None)
    {
        *charPtrPtr = NULL;
        return 1;
    }

    /*See if it's a string or a buffer object*/
    if (PyObject_AsCharBuffer(objPtr, charPtrPtr, &length) == -1)
    {
        /*See if it's an array*/
        PyErr_Clear();
        if (PyObject_AsWriteBuffer(objPtr, charPtrPtr, &length) == -1)
            return 0;
    }
    /*This code isn't necessary for a string, but it is for arrays and buffers,
      so we do it always anyway, since the PyObject_AsCharBuffer apparently doesn't
      guarantee us null-terminated data, and this way releasePointerString() doesn't
      have to differentiate */
    newPtr = malloc(length+1);
    if (newPtr == NULL)
    {
        PyErr_NoMemory();
        return 0;
    }
    memcpy(newPtr, *charPtrPtr, length);
    newPtr[length] = 0;
    *charPtrPtr = newPtr;
    return 1;
}

static void releasePointer(PyObject* objPtr, unsigned char* bytesPtr)
{
}

static void releasePointerString(PyObject* objPtr, char* charPtr)
{
    free(charPtr);
}

static PyObject* processStatus(int status)
{
    PyObject* o = NULL;

    /* If an error has already occurred, ignore the status and just fall through */
    if (PyErr_Occurred())
        return(NULL);

    if (status >= CRYPT_OK)
        return(Py_BuildValue(""));
"""
    pyBeforeFuncs = """\
    PyErr_SetObject(CryptException, o);
    Py_DECREF(o);
    return(NULL);
}

static int processStatusBool(int status)
{
    PyObject* o = processStatus(status);
    if (o == NULL)
        return(0);
    else
    {
        Py_DECREF(o);
        return(1);
    }
}

static PyObject* processStatusReturnInt(int status, int returnValue)
{
    PyObject* o = processStatus(status);
    if (o == NULL)
        return(0);
    else
    {
        Py_DECREF(o);
        o = Py_BuildValue("i", returnValue);
        return(o);
    }
}

static PyObject* processStatusReturnCryptHandle(int status, int returnValue)
{
    PyObject* o2;
    PyObject* o = processStatus(status);

    if (o == NULL)
        return(0);
    else
    {
        Py_DECREF(o);
        o2  = Py_BuildValue("(i)", returnValue);
        o = PyObject_CallObject(cryptHandleClass, o2);
        Py_DECREF(o2);
        return(o);
    }
}

static PyObject* processStatusReturnCryptQueryInfo(int status, CRYPT_QUERY_INFO returnValue)
{
    PyObject* o2;
    PyObject* o = processStatus(status);

    if (o == NULL)
        return(0);
    else
    {
        Py_DECREF(o);
        o2  = Py_BuildValue("(siiii)", returnValue.algoName, returnValue.blockSize, returnValue.minKeySize, returnValue.keySize, returnValue.maxKeySize);
        o = PyObject_CallObject(cryptQueryInfoClass, o2);
        Py_DECREF(o2);
        return(o);
    }
}

static PyObject* processStatusReturnCryptObjectInfo(int status, CRYPT_OBJECT_INFO returnValue)
{
    PyObject* o2;
    PyObject* o = processStatus(status);

    if (o == NULL)
        return(0);
    else
    {
        Py_DECREF(o);
        o2  = Py_BuildValue("(iiiis#)", returnValue.objectType, returnValue.cryptAlgo, returnValue.cryptMode, returnValue.hashAlgo, returnValue.salt, returnValue.saltSize);
        o = PyObject_CallObject(cryptObjectInfoClass, o2);
        Py_DECREF(o2);
        return(o);
    }
}
"""
    pyBeforeModuleFunctions = """

static PyMethodDef module_functions[] =
{
"""
    pyBeforeInts = r"""    {0, 0}
};

DL_EXPORT(void) initcryptlib_py(void)
{
    PyObject* module;
    PyObject* moduleDict;

    PyObject* v = NULL;
    PyObject *globalsDict;

    module = Py_InitModule("cryptlib_py", module_functions);
    moduleDict = PyModule_GetDict(module);

    CryptException = PyErr_NewException("cryptlib_py.CryptException", NULL, NULL);
    PyDict_SetItemString(moduleDict, "CryptException", CryptException);

    globalsDict = PyEval_GetGlobals();
    PyRun_String(
"from array import *\n\
import types\n\
class CryptHandle:\n\
    def __init__(self, value):\n\
        self.__dict__['value'] = value\n\
    def __int__(self):\n\
        return self.value\n\
    def __str__(self):\n\
        return str(self.value)\n\
    def __repr__(self):\n\
        return str(self.value)\n\
    def __getattr__(self, name):\n\
        name = name.upper()\n\
        if not name.startswith('CRYPT_'):\n\
            name = 'CRYPT_' + name\n\
        nameVal = globals()[name]\n\
        try:\n\
            return cryptGetAttribute(self, nameVal)\n\
        except CryptException, ex:\n\
            length = cryptGetAttributeString(self, nameVal, None)\n\
            buf = array('c', '0' * length)\n\
            length = cryptGetAttributeString(self, nameVal, buf)\n\
            return ''.join(buf[:length])\n\
    def __setattr__(self, name, value):\n\
        name = name.upper()\n\
        if not name.startswith('CRYPT_'):\n\
            name = 'CRYPT_' + name\n\
        nameVal = globals()[name]\n\
        if isinstance(value, types.IntType) or isinstance(value, CryptHandle):\n\
            cryptSetAttribute(self, nameVal, value)\n\
        else:\n\
            cryptSetAttributeString(self, nameVal, value)\n",
    Py_file_input, globalsDict, moduleDict);


    PyRun_String(
"class CRYPT_QUERY_INFO:\n\
    def __init__(self, algoName, blockSize, minKeySize, keySize, maxKeySize):\n\
        self.algoName = algoName\n\
        self.blockSize = blockSize\n\
        self.minKeySize = minKeySize\n\
        self.keySize = keySize\n\
        self.maxKeySize = maxKeySize\n",
    Py_file_input, globalsDict, moduleDict);

    PyRun_String(
"class CRYPT_OBJECT_INFO:\n\
    def __init__(self, objectType, cryptAlgo, cryptMode, hashAlgo, salt):\n\
        self.objectType = objectType\n\
        self.cryptAlgo = cryptAlgo\n\
        self.cryptMode = cryptMode\n\
        self.hashAlgo = hashAlgo\n\
        self.salt = salt\n",
    Py_file_input, globalsDict, moduleDict);

    cryptHandleClass = PyMapping_GetItemString(moduleDict, "CryptHandle");
    cryptQueryInfoClass = PyMapping_GetItemString(moduleDict, "CRYPT_QUERY_INFO");
    cryptObjectInfoClass = PyMapping_GetItemString(moduleDict, "CRYPT_OBJECT_INFO");

    Py_DECREF(globalsDict);

"""
    pyAfterInts = "}"

    paramCharPtrTemplate = "%(name)s, # string\n"
    paramIntTemplate = "%(name)s, # integer\n"
    paramIntTypeTemplate = "%(name)s, # %(type)s\n"
    paramEnumTypeTemplate = "%(name)s, # %(type)s\n"
    commentPrefix = "# "  #pad to 2-characters for formatting consistency
    classPrefix = "class crypt:\n"
    classPostfix= ""
    outFile = os.path.join(outDir, "cryptlib.py")
    sFuncs = "\n"
    sInts = "\n"
    sModFuncs = ""
    setupPy = r"""#!/usr/bin/env python

from distutils.core import setup, Extension
import sys

if sys.platform == "win32":
    ext = Extension("cryptlib_py",
                    sources=["python.c"],
                    library_dirs=['../binaries'],
                    libraries=['cl32'])
else:
    ext = Extension("cryptlib_py",
                    sources=["python.c"],
                    library_dirs=['..'],
                    libraries=['cl'])

setup(name="cryptlib_py", ext_modules=[ext])
"""

elif language == "net":
    typedefEnumTemplate = "// %(typedefName)s"
    typedefEnumElementTemplate = "public const int %(name)-NPADs = %(value)-VPADs;"

    typedefEnumElementTemplateComment = typedefEnumElementTemplate + " // %(comment)s"
    simpleEnumElementTemplate = "public const int %(name)-NPADs = %(value)-VPADs;"
    simpleEnumElementTemplateComment = simpleEnumElementTemplate + " // %(comment)s"
    defineNPad = "40"
    defineVPad = "4"
    defineTemplate = simpleEnumElementTemplate
    defineTemplateComment = simpleEnumElementTemplateComment
    exceptionPrefix = """
[StructLayout(LayoutKind.Sequential, Pack=0, CharSet=CharSet.Ansi)]
public class CRYPT_QUERY_INFO
{
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst=64)]public String algoName;
    public int blockSize;
    public int minKeySize;
    public int keySize;
    public int maxKeySize;

    public CRYPT_QUERY_INFO(){}

    public CRYPT_QUERY_INFO(String newAlgoName, int newBlockSize, int newMinKeySize, int newKeySize, int newMaxKeySize)
    {
        algoName = newAlgoName;
        blockSize = newBlockSize;
        minKeySize = newMinKeySize;
        keySize = newKeySize;
        maxKeySize = newMaxKeySize;
    }
}

[StructLayout(LayoutKind.Sequential, Pack=0, CharSet=CharSet.Ansi)]
public class CRYPT_OBJECT_INFO
{
    public int objectType;
    public int cryptAlgo;
    public int cryptMode;
    public int hashAlgo;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst=32)]public byte[] salt;
    public int saltSize;

    public CRYPT_OBJECT_INFO()
    {
        salt = new byte[64];
        saltSize = 64;
    }

    public CRYPT_OBJECT_INFO(int newObjectType, int newCryptAlgo, int newCryptMode, int newHashAlgo, byte[] newSalt)
    {
        objectType = newObjectType;
        cryptAlgo = newCryptAlgo;
        cryptMode = newCryptMode;
        hashAlgo = newHashAlgo;
    }
}

public class CryptException : ApplicationException
{
    public int Status { get { return (int)Data["Status"]; } }

    public int ExtraInfo { get { return (int)Data["ExtraInfo"]; } }

    public CryptException(int status)
        : base(convertMessage(status))
    {
        Data.Add("Status", status);
    }

    public CryptException(int status, int extra)
        : base(convertMessage(status))
    {
        Data.Add("Status", status);
        Data.Add("ExtraInfo", extra);
    }

    private static string convertMessage(int status)
    {
        String prefix = Convert.ToString(status) + ": ";
        switch (status)
        {
"""
    exceptionPostfix = """\
            default: return prefix + "Unknown Exception ?!?!";
        }
    }
}"""
    exceptionTemplate = """\
		case crypt.%(name)s: return prefix + "%(comment)s";
"""
    addFunctionWrappers = 1
    wholeFunctionDeclaration = None
    functionDeclaration = "public static "
    returnIntDeclaration = "int "
    returnVoidDeclaration = "void "
    paramsPrefix = "("
    paramsPostfix = ");"
    paramWhiteSpace = "\t\t\t\t\t\t"
    paramVoidPtrTemplate = "byte[] %(name)s,\n"
    addFunctionAlternate = 0
    paramCharPtrTemplate = "String %(name)s,\n"
    paramIntTemplate = "int %(name)s,\n"
    paramIntTypeTemplate = "int %(name)s, // %(type)s\n"
    wrapperLengthTemplate = "%(1)s == null ? 0 : %(1)s.Length, "
    wrapperStringLengthTemplate = "%(1)s == null ? 0 : new UTF8Encoding().GetByteCount(%(1)s), "
    wrapperStringReplace = ("byte[]", "String")
    wrapperStringTemplate = "%(1)s == null ? null : new UTF8Encoding().GetBytes(%(1)s)"
    #ENUM IDIOM
    #paramEnumTypeTemplate = "%(type)s %(name)s,\n"
    #ENUMs as ints
    paramEnumTypeTemplate = "int %(name)s, // %(type)s\n"
    commentPrefix = "//"
    classPrefix = """using System;
using System.Runtime.InteropServices;
using System.Text;

namespace cryptlib
{

public class crypt
{
    """
    classPostfix = """
    /* Helper Functions */

    private static void processStatus(int status)
    {
        if (status < crypt.OK)
            throw new CryptException(status);
    }


    private static void processStatus(int status, int extraInfo)
    {
        if (status < crypt.OK)
            throw new CryptException(status, extraInfo);
    }

    private static void checkIndices(byte[] array, int sequenceOffset, int sequenceLength)
    {
        if (array == null)
        {
            if (sequenceOffset == 0)
                return;
            else
                throw new IndexOutOfRangeException();
        }

        int arrayLength = array.Length;

        if (sequenceOffset < 0 ||
            sequenceOffset >= arrayLength ||
            sequenceOffset + sequenceLength > arrayLength)
            throw new IndexOutOfRangeException();
    }

    private static void getPointer(byte[] buffer, int bufferOffset, ref GCHandle bufferHandle, ref IntPtr bufferPtr)
    {
        if (buffer == null)
            return;
        bufferHandle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
        bufferPtr = Marshal.UnsafeAddrOfPinnedArrayElement(buffer, bufferOffset);
    }

    private static void releasePointer(GCHandle bufferHandle)
    {
        if (bufferHandle.IsAllocated)
            bufferHandle.Free();
    }
}
"""
    sFuncs = None
    sInts = None



s = open(inFile).read()
inFileTabSize = 4

#Global variables
#These accumulate information about the types in the input file
#----------------------------------------------------------------
nameSpace = {}  #Accumulate enum'd values here and use them to eval() the RHS of new ones
enumTypes = []  #Accumulate typedef'd enum types here
intTypes = []   #Accumulate typedef'd int types here
structTypes = []#Accumulate typedef'd struct types here
rawTypes = ["char", "int", "void"]
functionNames = [] #Accumulate function names here
rawParamStructsDict = {} #Accumulate function name -> list of ParamStructs (of original c code)
newParamStructsDict = {} #Accumulate function name -> list of ParamStructs (of new java/python code)
newReturnStructsDict = {}  #Accumulate function name -> ParamStruct of return parameter (of new java/python code)
newDiscardedStructsDict = {} #Accumulate function name -> ParamStruct of discarded parameter
lengthIndicesDict = {} #Accumulate function name -> indices in newParamStructs of lengths (used for python)
offsetIndicesDict= {} #Accumulate function name -> indices in newParamStructs of offsets (used for python)
errors = {}     #Dictionary mapping return values to exception objects

print "Parsing input file and generating %s files..." % language

#Removing enclosing include guard
#---------------------------------
s = re.search(r"#ifndef _CRYPTLIB_DEFINED\n(.*)\n#endif", s, re.DOTALL).group(1)

#Ignore anything before "#define C_INOUT..."
#--------------------------------------------
s = s[re.search(r"#define C_INOUT.*?\n", s, re.DOTALL).end() : ]

#Remove all conditionals
#------------------------
while 1:
    endifMatch = re.search(r"#endif.*?\n", s, re.DOTALL)
    if endifMatch == None:
        break
    ifdefIndex = s.rfind("#if", 0, endifMatch.start())
    s = s[ : ifdefIndex] + s[endifMatch.end() : ]


#Delete lines used for extended function and function-parameter checking
#like C_CHECK_RETVAL C_NONNULL_ARG( ( 3 ) ) \
#--------------------------------------------
functionParamterPattern = re.compile(r"((C_CHECK_RETVAL|C_NONNULL_ARG\s*\(\s*\([ \t0-9,]+\s*\)\s*\))\s+(\\\n)?)", re.DOTALL)
while 1:
	deleteExtended = functionParamterPattern.search(s)
	if deleteExtended == None:
		break
	s = s[ : deleteExtended.start(1) ] + s[ deleteExtended.end(1) : ]


#Replace typedef enums
#----------------------
typedefEnumPattern = re.compile(r"typedef[ \t]+enum[ \t]+{([^}]*)}\s*(\w*);", re.DOTALL)   # typedef enum { %1 } %2;

#Find the next typedef enum
typedefEnumMatch = typedefEnumPattern.search(s)
while typedefEnumMatch:

    #Extract its contents and type name
    enumContents, typedefName = typedefEnumMatch.groups()
    enumTypes.append(typedefName)

    #Parse its contents
    enumTuples = parseEnumContents(enumContents, nameSpace, "typedef")

    #Determine the length to pad names to
    namePad = str( max( [len(e[0]) for e in enumTuples] ) )
    valuePad = str( max( [len(e[1]) for e in enumTuples] ) )

    #Construct its output equivalent from language-specific string templates
    newTypedefEnum = typedefEnumTemplate % vars()
    for enumTuple in enumTuples:
        name, value, comment = enumTuple
        if not comment:
            paddedTemplate = typedefEnumElementTemplate.replace("NPAD", namePad).replace("VPAD", valuePad)
            newEnum = paddedTemplate % vars()
        else:
            paddedTemplate = typedefEnumElementTemplateComment.replace("NPAD", namePad).replace("VPAD", valuePad)
            newEnum = paddedTemplate % vars()
        if newTypedefEnum:
            newTypedefEnum += "\n"  #Don't always add this or we'll get extraneous newlines in python
        newTypedefEnum += newEnum

    #print "Output Equivalent of Typedef Enum====\n", newTypedefEnum
    #raw_input()

    if sInts:
        sInts += newTypedefEnum + "\n"

    #Substitute the output equivalent for the input
    s = s[ : typedefEnumMatch.start()] + newTypedefEnum + s[typedefEnumMatch.end() : ]

    #Get next typedef
    typedefEnumMatch = typedefEnumPattern.search(s, typedefEnumMatch.start() + len(newTypedefEnum))
#print "ENUMTYPES:\n",enumTypes

#Replace simple enums
#---------------------
#This works on the theory that we've already replaced all typedef enums with
#something that doesn't use the word "enum", so any remaining occurrences of
#enum will belong to simple enums.

simpleEnumPattern = re.compile(r"enum[ \t]+{([^}]*)};", re.DOTALL)            # enum { %1 };

#Find the next simple enum
simpleEnumMatch = simpleEnumPattern.search(s)
while simpleEnumMatch:

    #Extract its contents
    enumContents = simpleEnumMatch.group(1)

    #Parse its contents
    enumTuples = parseEnumContents(enumContents, nameSpace, "simple")

    #Determine the length to pad names to
    namePad = str( max( [len(e[0]) for e in enumTuples] ) )
    valuePad = str( max( [len(e[1]) for e in enumTuples] ) )

    #Construct its output equivalent from language-specific string templates
    newSimpleEnum = ""
    for enumTuple in enumTuples:
        name, value, comment = enumTuple
        if not comment:
            paddedTemplate = simpleEnumElementTemplate.replace("NPAD", namePad).replace("VPAD", valuePad)
            newEnum = paddedTemplate % vars()
        else:
            paddedTemplate = simpleEnumElementTemplateComment.replace("NPAD", namePad).replace("VPAD", valuePad)
            newEnum = paddedTemplate % vars()
        if newSimpleEnum:
            newSimpleEnum += "\n"   #Don't always add this or we'll get extraneous newlines in python
        newSimpleEnum += newEnum

    #print "Output Equivalent of Simple Enum====\n", newSimpleEnum
    #raw_input()

    if sInts:
        sInts += newSimpleEnum + "\n"

    #Substitute the output equivalent for the input
    s = s[ : simpleEnumMatch.start()] + newSimpleEnum + s[simpleEnumMatch.end() : ]

    #Get next typedef
    simpleEnumMatch = simpleEnumPattern.search(s, simpleEnumMatch.start() + len(newSimpleEnum))


#Replace #define'd constants
#----------------------------
definePattern = re.compile(r"#define[ \t]+(\w+)[ \t]+([-\w]+)[ \t]*(/\*.*\*/*)?")    # #define %1 %2 [/*%3*/]

exceptionString = exceptionPrefix

#Find the next #define
defineMatch = definePattern.search(s)
while defineMatch:

    #Parse its contents
    name, value, comment = defineMatch.groups()
    if not name.startswith("CRYPT_"):
        raise "name doesn't start with CRYPT_"+name
    name = name.replace("CRYPT_", "")

    #Construct its output equivalent from language-specific string templates
    if not comment:
        paddedTemplate = defineTemplate.replace("NPAD", defineNPad).replace("VPAD", defineVPad)
        newDefine =  paddedTemplate % vars()
    else:
        comment = comment[2:-2].strip()
        paddedTemplate = defineTemplateComment.replace("NPAD", defineNPad).replace("VPAD", defineVPad)
        newDefine = paddedTemplate % vars()

    #print "define: " + newDefine
    #raw_input()

    if sInts:
        sInts += newDefine + "\n"

    #Substitute the output equivalent for the input
    s = s[ : defineMatch.start()] + newDefine + s[defineMatch.end() : ]

    #Append to exception string if error
    if name.startswith("ERROR_") or name.startswith("ENVELOPE_RESOURCE"):
        exceptionString += exceptionTemplate % vars()

    #Get next #define
    defineMatch = definePattern.search(s, defineMatch.start() + len(newDefine))

exceptionString += exceptionPostfix

#Replace #define'd constants with parenthesis
#--------------------------------------------
definePattern = re.compile(r"#define[ \t]+(\w+)[ \t]+\([ \t]*([-0-9]+)[ \)]*\)[ \t]*(/\*.*\*/*)?")    # #define %1 ( %2 ) [/*%3*/]

exceptionString = exceptionPrefix

#Find the next #define
defineMatch = definePattern.search(s)
while defineMatch:

    #Parse its contents
    name, value, comment = defineMatch.groups()
    if not name.startswith("CRYPT_"):
        raise "name doesn't start with CRYPT_"+name
    name = name.replace("CRYPT_", "")

    #Construct its output equivalent from language-specific string templates
    if not comment:
        paddedTemplate = defineTemplate.replace("NPAD", defineNPad).replace("VPAD", defineVPad)
        newDefine =  paddedTemplate % vars()
    else:
        comment = comment[2:-2].strip()
        paddedTemplate = defineTemplateComment.replace("NPAD", defineNPad).replace("VPAD", defineVPad)
        newDefine = paddedTemplate % vars()

    #print "define: " + newDefine
    #raw_input()

    if sInts:
        sInts += newDefine + "\n"

    #Substitute the output equivalent for the input
    s = s[ : defineMatch.start()] + newDefine + s[defineMatch.end() : ]

    #Append to exception string if error
    if name.startswith("ERROR_") or name.startswith("ENVELOPE_RESOURCE"):
        exceptionString += exceptionTemplate % vars()

    #Get next #define
    defineMatch = definePattern.search(s, defineMatch.start() + len(newDefine))

exceptionString += exceptionPostfix

#Comment out #define'd macros
#-----------------------------
definePattern = re.compile(r"#define[ \t]+[^(]+\([^\)]*\)([^\n]*\\\n)*[^\n]*")
defineMatch = definePattern.search(s)
while defineMatch:
    #print "defined macros:", defineMatch.group()
    #raw_input()
    define = defineMatch.group()
    newDefine = commentPrefix + "CRYPTLIBCONVERTER - NOT SUPPORTED:\n" + define
    newDefine = newDefine.replace("\n", "\n"+commentPrefix)
    s = s[ : defineMatch.start()] + newDefine + s[defineMatch.end() : ]
    defineMatch = definePattern.search(s, defineMatch.start() + len(newDefine))

#Comment out typedef integer types
#----------------------------------
typedefIntPattern = re.compile(r"typedef[ \t]+int[ \t]+([^ \t]*)[ \t]*;[ \t]*\n")
typedefIntMatch = typedefIntPattern.search(s)
while typedefIntMatch:
    typedefInt = typedefIntMatch.group()
    typedefIntName = typedefIntMatch.group(1)
    intTypes.append(typedefIntName)
    #print "typedef int:", typedefInt
    #raw_input()
    newTypedefInt = commentPrefix + "CRYPTLIBCONVERTER - NOT NEEDED: " + typedefInt
    s = s[ : typedefIntMatch.start()] + newTypedefInt + s[typedefIntMatch.end() : ]
    typedefIntMatch = typedefIntPattern.search(s, typedefIntMatch.start() + len(newTypedefInt))
#print "INTTYPES:\n",intTypes

#Comment out typedef structs
#----------------------------
typedefStructPattern = re.compile(r"typedef[ \t]+struct[ \t]\{[^}]*}[ \t]*([^;]+);")
typedefStructMatch = typedefStructPattern.search(s)
while typedefStructMatch:
    typedefStruct = typedefStructMatch.group()
    typedefStructName = typedefStructMatch.group(1)
    structTypes.append(typedefStructName)
    #print "typedef struct:", typedefStructName
    #raw_input()
    newTypedefStruct = commentPrefix + "CRYPTLIBCONVERTER - NOT SUPPORTED:\n" + typedefStruct
    newTypedefStruct = newTypedefStruct.replace("\n", "\n"+commentPrefix)
    s = s[ : typedefStructMatch.start()] + newTypedefStruct + s[typedefStructMatch.end() : ]
    typedefStructMatch = typedefStructPattern.search(s, typedefStructMatch.start() + len(newTypedefStruct))
#print "STRUCTTYPES:\n",structTypes
#raw_input()

#Translate functions
#--------------------
functionPattern = re.compile(r"C_RET[ \t]+([^ \t]+)[ \t]*\(([^\)]*)\);", re.DOTALL)
functionMatch = functionPattern.search(s)
while functionMatch:
    function = functionMatch.group()
    functionName, functionParams = functionMatch.groups()
    if not functionName.startswith("crypt"):
        raise "name doesn't start with crypt"+functionName
    functionName = functionName[len("crypt") : ]
    functionNames.append(functionName)
    #print "function:", functionName, functionParams
    #raw_input()

    #Parse its parameters
    paramStructs = parseFunctionParams(functionParams)

    #Add raw list of ParamStructs to dictionary
    #This will be used later, when generating the .c glue code
    rawParamStructsDict[functionName] = paramStructs

    # Since java and python don't have pass-by-reference, what we do is migrate the
    # output int parameter in certain functions into the return value.  If the function
    # creates or returns values such as integers or a (void*, int*) pair, we record
    # the indexes of the parameters to migrate.  We do this in functions like:
    # cryptCreate*()            # return handle of new object
    # cryptGetAttribute()       # return integer value of attribute
    # cryptGetAttributeString() # convert (void*, int*) to python string or java byte array
    # cryptExportKey()          # convert (void*, int*) "
    # cryptCreateSignature()    # convert (void*, int*)
    # cryptKeysetOpen()         # return handle
    # cryptGetPublicKey()       # return handle
    # cryptGetPrivateKey()      # return handle
    # cryptGetCertExtension()   # convert (void*, int) but DISCARD criticalFlag (the int* before)!
    # cryptImportCert()         # return handle
    # cryptExportCert()         # convert (void*, int)
    # cryptCAGetItem()          # return handle
    # cryptCACertManagement()   # return handle
    # cryptPushData()           # return integer (# of bytes copied)
    # cryptPopData()            # convert (void*, int*) even though they're not adjacent
    discardIntIndex = -1
    returnIntIndex = -1
    returnVoidPtrIndex = -1
    discardInLengthIndex1 = -1   # To discard input lengths, since python don't need these
    discardInLengthIndex2 = -1

    #Scan through looking for a void pointer preceded by "keyIDtype"
    #Convert it into a char*
    index = 1
    for p in paramStructs[1:]:
        p2 = paramStructs[index-1]
        if p.isPtr and p.type=="void" and p2.type=="CRYPT_KEYID_TYPE":
            p.type = "char"
        index += 1

    #Scan through looking for output int pointers (or structs)?!
    #If there's two, we will migrate the second occurrence to the return value
    #and discard the first so as to handle cryptGetCertExtension()
    index = 0
    for p in paramStructs:
        if p.isOut and p.isPtr and (p.category=="intType" or p.type=="int" or p.category=="structType"):
            if returnIntIndex != -1:
                if discardIntIndex != -1:
                    raise "Found two returned ints to discard?!"
                discardIntIndex = returnIntIndex
            returnIntIndex = index
        index += 1

    #Record the migrated return value's ParamStruct
    if returnIntIndex != -1:
        newReturnStructsDict[functionName] = paramStructs[returnIntIndex]

    #Return the discarded value's ParamStruct
    if discardIntIndex != -1:
        newDiscardedStructsDict[functionName] = paramStructs[discardIntIndex]

    #Copy the parsed parameters and remove those we're going to migrate or discard
    newParamStructs = [paramStructs[count]
                       for count in range(len(paramStructs))
                       if count not in (discardIntIndex, returnIntIndex)]

    #Indices of input offsets and lengths
    offsetIndices = []
    lengthIndices = []

    #Scan through looking for void pointer, add an int offset
    index = 0
    while 1:
        if index >= len(newParamStructs):
            break
        p = newParamStructs[index]
        if p.isPtr and p.type=="void":
            newp = ParamStruct()
            newp.type = "int"
            newp.isPtr = 0
            newp.isOut = 0
            newp.category = "rawType"
            newp.name = p.name+"Offset"
            newParamStructs = newParamStructs[:index+1] + [newp] + newParamStructs[index+1:]
            offsetIndices.append(index+1)
            if not p.isOut and len(newParamStructs)> index+2 and newParamStructs[index+2].type == "int":
                lengthIndices.append(index+2)
        index += 1

    #Add processed list of ParamStructs to dictionary
    #This will be used later, when generating the .c glue code
    newParamStructsDict[functionName] = newParamStructs
    lengthIndicesDict[functionName] = lengthIndices
    offsetIndicesDict[functionName] = offsetIndices

    #Used for Python
    if wholeFunctionDeclaration:
        newFunction = wholeFunctionDeclaration % functionName
        if sFuncs:
            sFuncs += newFunction + "\n"
        else: assert(0)
        sModFuncs += (moduleFunctionEntry % (functionName, functionName)) + "\n"
    else: #Java

        #Construct new function declaration from language-specific templates
        newFunction = functionDeclaration
        if returnIntIndex != -1:
            if newReturnStructsDict.get(functionName).category == "structType":
                newFunction += newReturnStructsDict.get(functionName).type + " "
            else:
                newFunction += returnIntDeclaration
        else:
            newFunction += returnVoidDeclaration
        newFunction += functionName + paramsPrefix
        if len(newParamStructs):
            newFunction += "\n" + paramWhiteSpace
        if offsetIndices or lengthIndices:
            newFunctionWrapper = newFunction
            newFunctionWrapperArgs = ""
        else:
            newFunctionWrapper = None
        #At this point I'm just getting lazy.  Basically, we automatically generate
        #String wrappers for function that take void* input parameters.  Encrypt and
        #Decrypt take void* in/out parameters.  We should be smart enough to detect
        #these and not generate wrappers for them, but instead, since we don't otherwise
        #differentiate between C_IN and C_INOUT, we just hardcodedly exclude these
        #from having String wrappers
        if offsetIndices and lengthIndices and functionName not in ("Encrypt", "Decrypt"):
            newFunctionStringWrapper = newFunction
            newFunctionStringWrapperArgs = ""
        else:
            newFunctionStringWrapper = None
            newFunctionStringWrapperArgs = ""
        index = 0
        for p in newParamStructs:
            if p.category == "rawType" and p.type == "void" and p.isPtr:
                template = paramVoidPtrTemplate
                previousBufferName = p.name
            elif p.category == "rawType" and p.type == "char" and p.isPtr:
                template = paramCharPtrTemplate
            elif p.category == "rawType" and p.type == "int" and not p.isPtr:
                template = paramIntTemplate
            elif p.category == "intType" and not p.isPtr:
                template = paramIntTypeTemplate
            elif p.category == "enumType" and not p.isPtr:
                template = paramEnumTypeTemplate
            #elif p.category == "structType"
            else:
                raise "Unrecognized parameter type! " + str(p)

            #Strip out commas from the last param template, kind of a hack..
            if index == len(newParamStructs)-1:
                template = template[:].replace(",", "")

            #Update the main function with this new param
            newFunction += template % vars(p)
            newFunction += paramWhiteSpace

            #Update the wrapper function with this new param
            #If this is not a special param...
            if addFunctionWrappers and newFunctionWrapper and \
                index not in offsetIndices and index not in lengthIndices:
                #Determine if this is the last param in the wrapper function
                anyMoreParams = [e for e in range(index+1, len(newParamStructs))
                                 if e not in offsetIndices and e not in lengthIndices]

                #Update the wrapper function's param list
                if not anyMoreParams:
                    template = template[:].replace(",", "")
                newFunctionWrapper += template % vars(p)
                newFunctionWrapper += paramWhiteSpace

                #Update the wrapper function's args that it uses to call the main function
                newFunctionWrapperArgs += p.name
                newFunctionWrapperArgs += ", "

                #Do the same things for the string wrapper
                if newFunctionStringWrapper:
                    newFunctionStringWrapper += template % vars(p)
                    newFunctionStringWrapper += paramWhiteSpace
                    if p.type=="void" and p.isPtr and not p.isOut:
                        newFunctionStringWrapperArgs += wrapperStringTemplate % {"1":p.name};
                    else:
                        newFunctionStringWrapperArgs += p.name
                    newFunctionStringWrapperArgs += ", "
            else:
                #If this is a special param (an offset or length)...
                if index in offsetIndices:
                    newFunctionWrapperArgs += "0, "
                    newFunctionStringWrapperArgs += "0, "
                elif index in lengthIndices:
                    newFunctionWrapperArgs += wrapperLengthTemplate % {"1":previousBufferName}
                    newFunctionStringWrapperArgs += wrapperStringLengthTemplate % {"1":previousBufferName}
            index += 1

        #Add final postfix of ); to the main and wrapper param lists
        newFunction += paramsPostfix
        if newFunctionWrapper:
            newFunctionWrapper += paramsPostfix
        if newFunctionStringWrapper:
            newFunctionStringWrapper += paramsPostfix

        #If this function takes a void* parameter, and we want to convert this
        #into two different versions, duplicate and modify the version here
        if addFunctionAlternate:
            if newFunction.find(paramVoidPtrAlternate[0]) != -1:
                newFunction += "\n" + newFunction.replace(*paramVoidPtrAlternate)

        #If we've prepared a wrapper function to eliminate need for offset[, length]:
        if addFunctionWrappers and newFunctionWrapper:
            newFunctionWrapper = newFunctionWrapper.replace("native ", "")
            newFunctionWrapper = newFunctionWrapper[:-1] + " { return %s(%s); }" % (functionName, newFunctionWrapperArgs[:-2])
            if returnIntIndex == -1:
                newFunctionWrapper = newFunctionWrapper.replace("return ", "")
            #Duplicate and modify the wrapper function
            if addFunctionAlternate:
                newFunctionWrapper2 = newFunctionWrapper.replace(*paramVoidPtrAlternate)
                newFunctionWrapper2 = newFunctionWrapper2.replace("capacity()", "length")
                newFunctionWrapper += "\n" + newFunctionWrapper2
            newFunction += "\n" + newFunctionWrapper
        if addFunctionWrappers and newFunctionStringWrapper:
            newFunctionStringWrapper = newFunctionStringWrapper.replace("native ", "")
            newFunctionStringWrapper = newFunctionStringWrapper[:-1] + " { return %s(%s); }" % (functionName, newFunctionStringWrapperArgs[:-2])
            newFunctionStringWrapper = newFunctionStringWrapper.replace(*wrapperStringReplace)
            if returnIntIndex == -1:
                newFunctionStringWrapper = newFunctionStringWrapper.replace("return ", "")
            newFunction += "\n" + newFunctionStringWrapper

        #Add a special-case string accessor for GetAttributeString() to Java and .NET
        if functionName == "GetAttributeString" and language == "java":
            newFunction += """
public static String GetAttributeString(
                    int cryptHandle, // CRYPT_HANDLE
                    int attributeType // CRYPT_ATTRIBUTE_TYPE
                    ) throws CryptException
                    {
                        int length = GetAttributeString(cryptHandle, attributeType, (byte[])null);
                        byte[] bytes = new byte[length];
                        length = GetAttributeString(cryptHandle, attributeType, bytes);
                        return new String(bytes, 0, length);
                    }
"""
        elif functionName == "GetAttributeString" and language == "net":
            newFunction += """
public static String GetAttributeString(
                    int cryptHandle, // CRYPT_HANDLE
                    int attributeType // CRYPT_ATTRIBUTE_TYPE
                    )
                    {
                        int length = GetAttributeString(cryptHandle, attributeType, null);
                        byte[] bytes = new byte[length];
                        length = GetAttributeString(cryptHandle, attributeType, bytes);
                        return new UTF8Encoding().GetString(bytes, 0, length);
                    }
"""
        #Add special-case functions for cryptAddRandom(), since it allows NULL as a
        #first parameter, and a pollType constant in place of the length
        elif functionName == "AddRandom":
            if language == "java":
                newFunction += """
public static native void AddRandom(
                    int pollType
                    ) throws CryptException;
"""
            elif language == "net":
                newFunction += """
public static void AddRandom(
                    int pollType
                    );
"""

        # Add an extra linebreak between functions
        # This isn't in cryptlib.h, but it makes the converted files more readable
        if s[functionMatch.end()+1] != "\n":
            newFunction += "\n"

    s = s[ : functionMatch.start()] + newFunction + s[functionMatch.end() : ]
    functionMatch = functionPattern.search(s, functionMatch.start() + len(newFunction))

#Translate comments
#-------------------
if language != "java" and language != "net":
    while 1:
        match = re.search(r"/\*(.*?)\*/", s, re.DOTALL)
        if match == None:
            break
        #print match.group()
        #raw_input()
        commentStrings = (commentPrefix + match.group(1) + "  ").split("\n")
        newComment = commentStrings[0]
        for commentString in commentStrings[1:]:
            if commentString.startswith("\t"):
                newComment += "\n" + commentPrefix + (" " * (inFileTabSize-2)) + commentString[1:]
            elif commentString.startswith("  "):
                newComment += "\n" + commentPrefix + commentString[1:]
            else:
                newComment += "\n" + commentPrefix + commentString
            #print "building up:\n", newComment
            #raw_input()
        idx = commentString.find("\n")
        s = s[ : match.start()] + newComment + s[match.end() : ]

#Indent each line by one tab
#---------------------------
s = s.replace("\n", "\n\t")

#Write out file(s)
#Then generate the .h/.c files necessary for language integration
#At this point it's easier to just hardcode constants and have
#separate python and java codepaths
#---------------
if language=="java":

    #Add enclosing class syntax
    #---------------------------
    s = classPrefix + s + classPostfix

    os.chdir(outDir)
    if not os.path.exists("cryptlib"):
        os.mkdir("./cryptlib")
    os.chdir("./cryptlib")

    print "Writing java files..."
    f = open("crypt.java", "w")
    f.write(s)
    f.close()
    f = open("CryptException.java", "w")
    f.write(exceptionString)
    f.close()
    f = open("CRYPT_QUERY_INFO.java", "w")
    f.write(cryptQueryInfoString)
    f.close()
    f = open("CRYPT_OBJECT_INFO.java", "w")
    f.write(cryptObjectInfoString)
    f.close()


    print "Compiling java files..."
    print os.popen4("javac -classpath .. crypt.java")[1].read()                    #Compile java file
    print os.popen4("javac -classpath .. CryptException.java")[1].read()           #Compile java file
    print os.popen4("javac -classpath .. CRYPT_QUERY_INFO.java")[1].read()         #Compile java file
    print os.popen4("javac -classpath .. CRYPT_OBJECT_INFO.java")[1].read()        #Compile java file

    os.chdir("..")

    print "Generating jar file..."
    print os.popen4("jar cf cryptlib.jar cryptlib/crypt.class cryptlib/CryptException.class cryptlib/CRYPT_QUERY_INFO.class cryptlib/CRYPT_OBJECT_INFO.class cryptlib")[1].read()

    print "Generating JNI header file..."
    print os.popen4("javah -classpath . -o cryptlib_jni.h -jni cryptlib.crypt")[1].read()    #Generate C header

    s = open("cryptlib_jni.h").read()
    os.remove("cryptlib_jni.h")

    print "Generating JNI source file..."

    #Now we take cryptlib_jni.h and derive the .c file from it

    #Strip off headers and footers
    #startIndex = s.find("/*\n * Class")
    #endIndex = s.find("#ifdef", startIndex)
    #s = s[startIndex : endIndex]

    startIndex = s.find("#undef")
    endIndex = s.find("/*\n * Class", startIndex)
    sold = s
    s = s[startIndex : endIndex]

    startIndex = endIndex
    endIndex = sold.find("#ifdef", startIndex)
    s2 = sold[startIndex : endIndex]

    #Add includes and helper functions
    #Note that we assume "cryptlib.h" is one directory behind us
    s = r"""
#include "../crypt.h"

#ifdef USE_JAVA

#include <jni.h>
#include <stdio.h>  //printf
#include <stdlib.h> //malloc, free

%s

/* Helper Functions */

int processStatus(JNIEnv *env, jint status)
{
    jclass exClass;
    jmethodID exConstructor;
    jthrowable obj;

    if (status >= cryptlib_crypt_OK)
        return 1;

    exClass = (*env)->FindClass(env, "cryptlib/CryptException");
    if (exClass == 0)
    {
        printf("java_jni.c:processStatus - no class?!\n");
        return 0;
    }

    exConstructor = (*env)->GetMethodID(env, exClass, "<init>", "(I)V");
    if (exConstructor == 0)
    {
        printf("java_jni.c:processStatus - no constructor?!\n");
        return 0;
    }

    obj = (*env)->NewObject(env, exClass, exConstructor, status);
    if (obj == 0)
    {
        printf("java_jni.c:processStatus - no object?!\n");
        return 0;
    }

    if ((*env)->Throw(env, obj) < 0)
    {
        printf("java_jni.c:processStatus - failed to throw?!\n");
        return 0;
    }
    return 0;
}

jobject processStatusReturnCryptQueryInfo(JNIEnv *env, int status, CRYPT_QUERY_INFO returnValue)
{
    jclass exClass;
    jmethodID exConstructor;
    jstring algoName;
    jobject obj;

    if (status < cryptlib_crypt_OK)
        return NULL;

    exClass = (*env)->FindClass(env, "cryptlib/CRYPT_QUERY_INFO");
    if (exClass == 0)
    {
        printf("java_jni.c:processStatusReturnCryptQueryInfo - no class?!\n");
        return NULL;
    }

    exConstructor = (*env)->GetMethodID(env, exClass, "<init>", "(Ljava/lang/String;IIII)V");
    if (exConstructor == 0)
    {
        printf("java_jni.c:processStatusReturnCryptQueryInfo - no constructor?!\n");
        return NULL;
    }

    algoName = (*env)->NewStringUTF(env, returnValue.algoName);

    obj = (*env)->NewObject(env, exClass, exConstructor, algoName, returnValue.blockSize, returnValue.minKeySize, returnValue.keySize, returnValue.maxKeySize);
    if (obj == 0)
    {
        printf("java_jni.c:processStatusReturnCryptQueryInfo - no object?!\n");
        return NULL;
    }

    return obj;
}

jobject processStatusReturnCryptObjectInfo(JNIEnv *env, int status, CRYPT_OBJECT_INFO returnValue)
{
    jclass exClass;
    jmethodID exConstructor;
    jbyteArray salt;
    jobject obj;

    if (status < cryptlib_crypt_OK)
        return NULL;

    exClass = (*env)->FindClass(env, "cryptlib/CRYPT_OBJECT_INFO");
    if (exClass == 0)
    {
        printf("java_jni.c:processStatusReturnCryptObjectInfo - no class?!\n");
        return NULL;
    }

    exConstructor = (*env)->GetMethodID(env, exClass, "<init>", "(IIII[B)V");
    if (exConstructor == 0)
    {
        printf("java_jni.c:processStatusReturnCryptObjectInfo - no constructor?!\n");
        return NULL;
    }

    salt = (*env)->NewByteArray(env, returnValue.saltSize);
    (*env)->SetByteArrayRegion(env, salt, 0, returnValue.saltSize, returnValue.salt);

    obj = (*env)->NewObject(env, exClass, exConstructor, returnValue.objectType, returnValue.cryptAlgo, returnValue.cryptMode, returnValue.hashAlgo, salt);
    if (obj == 0)
    {
        printf("java_jni.c:processStatusReturnCryptObjectInfo - no object?!\n");
        return NULL;
    }

    return obj;
}

int checkIndicesArray(JNIEnv *env, jbyteArray array, int sequenceOffset, int sequenceLength)
{
    jsize arrayLength;
    jclass exClass;

    if (array == NULL)
    {
        if (sequenceOffset == 0)
            return 1;
        else
        {
            exClass = (*env)->FindClass(env, "java/lang/ArrayIndexOutOfBoundsException");
            if (exClass == 0)
                printf("java_jni.c:checkIndicesArray - no class?!\n");
            else
            if ((*env)->ThrowNew(env, exClass, "") < 0)
                printf("java_jni.c:checkIndicesArray - failed to throw?!\n");
            return 0;
        }
    }

    arrayLength = (*env)->GetArrayLength(env, array);

    if (sequenceOffset < 0 ||
        sequenceOffset >= arrayLength ||
        sequenceOffset + sequenceLength > arrayLength)
    {
        exClass = (*env)->FindClass(env, "java/lang/ArrayIndexOutOfBoundsException");
        if (exClass == 0)
            printf("java_jni.c:checkIndicesArray - no class?!\n");
        else
        if ((*env)->ThrowNew(env, exClass, "") < 0)
            printf("java_jni.c:checkIndicesArray - failed to throw?!\n");
        return 0;
    }
    return 1;
}

int getPointerArray(JNIEnv* env, jbyteArray array, jbyte** bytesPtrPtr)
{
    jboolean isCopy;

    if (array == NULL)
    {
        (*bytesPtrPtr) = NULL;
        return 1;
    }

    (*bytesPtrPtr) = (*env)->GetByteArrayElements(env, array, &isCopy);

    if (*bytesPtrPtr == NULL)
    {
        printf("java_jni.c:getPointer - failed to get elements of array?!\n");
        return 0;
    }
    return 1;
}

void releasePointerArray(JNIEnv* env,jbyteArray array, jbyte* bytesPtr)
{
    if (bytesPtr == NULL)
        return;
    (*env)->ReleaseByteArrayElements(env, array, bytesPtr, 0);
}

int checkIndicesNIO(JNIEnv *env, jobject byteBuffer, int sequenceOffset, int sequenceLength)
{
    jlong byteBufferLength;
    jclass exClass;

    if (byteBuffer == NULL)
    {
        if (sequenceOffset == 0)
            return 1;
        else
        {
            exClass = (*env)->FindClass(env, "java/lang/ArrayIndexOutOfBoundsException");
            if (exClass == 0)
                printf("java_jni.c:checkIndicesNIO - no class?!\n");
            else
            if ((*env)->ThrowNew(env, exClass, "") < 0)
                printf("java_jni.c:checkIndicesNIO - failed to throw?!\n");
            return 0;
        }
    }

    byteBufferLength = (*env)->GetDirectBufferCapacity(env, byteBuffer);
    if (byteBufferLength == -1)
    {
        exClass = (*env)->FindClass(env, "java/lang/UnsupportedOperationException");
        if (exClass == 0)
            printf("java_jni.c:checkIndicesNIO - no class?!\n");
        else
        if ((*env)->ThrowNew(env, exClass,
"Either a non-direct ByteBuffer was passed or your JVM doesn't support JNI access to direct ByteBuffers") < 0)
            printf("java_jni.c:checkIndicesNIO - failed to throw?!\n");
        return 0;
    }

    if (sequenceOffset < 0 ||
        sequenceOffset >= byteBufferLength ||
        sequenceOffset + sequenceLength > byteBufferLength)
    {
        exClass = (*env)->FindClass(env, "java/lang/ArrayIndexOutOfBoundsException");
        if (exClass == 0)
            printf("java_jni.c:checkIndicesNIO - no class?!\n");
        else
        if ((*env)->ThrowNew(env, exClass, "") < 0)
            printf("java_jni.c:checkIndicesNIO - failed to throw?!\n");
        return 0;
    }
    return 1;
}

int getPointerNIO(JNIEnv* env, jobject byteBuffer, jbyte** bytesPtrPtr)
{
    jclass exClass;

    if (byteBuffer == NULL)
    {
        (*bytesPtrPtr) = NULL;
        return 1;
    }

    (*bytesPtrPtr) = (*env)->GetDirectBufferAddress(env, byteBuffer);

    if (*bytesPtrPtr == NULL)
    {
        exClass = (*env)->FindClass(env, "java/lang/UnsupportedOperationException");
        if (exClass == 0)
            printf("java_jni.c:getPointerNIO - no class?!\n");
        else
        if ((*env)->ThrowNew(env, exClass, "Your JVM doesn't support JNI access to direct ByteBuffers") < 0)
            printf("java_jni.c:getPointerNIO - failed to throw?!\n");
        return 0;
    }
    return 1;
}

void releasePointerNIO(JNIEnv* env,jbyteArray array, jbyte* bytesPtr)
{
}

int getPointerString(JNIEnv* env, jstring str, jbyte** bytesPtrPtr)
{
   jboolean isCopy;
   jsize strLength;
   const jbyte* rawBytesPtr;
   jclass exClass;
#ifdef __WINCE__
   int status;
#endif // __WINCE__


   if (str == NULL)
   {
       (*bytesPtrPtr) = NULL;
       return 1;
   }

   rawBytesPtr = (*env)->GetStringUTFChars(env, str, &isCopy);

   if (rawBytesPtr == NULL)
   {
       printf("java_jni.c:getPointerString - failed to get elements of String?!\n");
       return 0;
   }

   strLength = (*env)->GetStringUTFLength(env, str);

#ifdef __WINCE__
   (*bytesPtrPtr) = (jbyte*)malloc(strLength*2+2); // this is unicode, therefore \0 is two bytes long
#else
   (*bytesPtrPtr) = (jbyte*)malloc(strLength+1);
#endif // __WINCE__

   if (*bytesPtrPtr == NULL)
   {
       exClass = (*env)->FindClass(env, "java/lang/OutOfMemoryError");
       if (exClass == 0)
           printf("java_jni.c:getPointerString - no class?!\n");
       else
       if ((*env)->ThrowNew(env, exClass, "") < 0)
           printf("java_jni.c:getPointerString - failed to throw?!\n");
       (*env)->ReleaseStringUTFChars(env, str, rawBytesPtr);
       return 0;
   }

#ifdef __WINCE__
   status = asciiToUnicode (*bytesPtrPtr, strLength*2+2, rawBytesPtr, strLength+1);
   if (status == CRYPT_ERROR_BADDATA) {
       (*env)->ReleaseStringUTFChars(env, str, rawBytesPtr);
       return 0;
   }
#else
   memcpy(*bytesPtrPtr, rawBytesPtr, strLength);
   (*bytesPtrPtr)[strLength] = 0;
#endif // __WINCE__

   (*env)->ReleaseStringUTFChars(env, str, rawBytesPtr);

   return 1;
}

void releasePointerString(JNIEnv* env, jstring str, jbyte* bytesPtr)
{
    if (bytesPtr == NULL)
        return;
    free(bytesPtr);
}

%s

#endif /* USE_JAVA */
""" % (s, s2)

    functionPattern = re.compile(r"JNIEXPORT ([^ \t]+) JNICALL Java_cryptlib_crypt_([^ \t\n]+)\n[ \t]*\(([^\)]*)\);")
    functionMatch = functionPattern.search(s)
    while functionMatch:
        #print functionMatch.groups()

        #Extract the returnType, name, and prototype for this function
        function = functionMatch.group()
        functionReturnType, functionName, functionPrototype = functionMatch.groups()

        #Handle special-case AddRandom(pollType) function
        if functionName == "AddRandom__I":
            p = ParamStruct()
            p.type = "int"
            p.isPtr = 0
            p.isOut = 0
            p.category = "intType"
            p.name = "NULL"
            p.rawIndex = 0
            p2 = ParamStruct()
            p2.type = "int"
            p2.isPtr = 0
            p2.isOut = 0
            p2.category = "intType"
            p2.name = "pollType"
            p2.rawIndex = 1
            rawParamStructs = [p,p2]
            newParamStructs = [p2,p2]
            voidTag = "Array"
            functionName = functionName.split("__")[0]
            returnName = None
            discardName = None
        else:
            #Strip JNI decoration off function name
            #But record which variety of tagged helper functions to use
            if functionName.find("__") != -1:
                if functionName.find("ByteBuffer") != -1:
                    voidTag = "NIO"
                else:
                    voidTag = "Array"
                functionName = functionName.split("__")[0]

            #Look up the parameters we've previously determined for this function
            rawParamStructs = rawParamStructsDict[functionName]
            newParamStructs = newParamStructsDict[functionName]
            if newReturnStructsDict.get(functionName):
                returnName = newReturnStructsDict.get(functionName).name
                returnType = newReturnStructsDict.get(functionName).type
                returnCategory = newReturnStructsDict.get(functionName).category
            else:
                returnName = None

            if newDiscardedStructsDict.get(functionName):
                discardName = newDiscardedStructsDict.get(functionName).name
            else:
                discardName = None

        #for p in newParamStructs: print "     "+str(p)

        #Substitute parameter names into the function prototype
        newFunctionParams = expandFunctionPrototype(functionPrototype, newParamStructs)
        startIndex = functionMatch.start(3) - functionMatch.start()
        endIndex = functionMatch.end(3) - functionMatch.start()
        function = function[ : startIndex] + newFunctionParams + function[ endIndex : ]

        newFunctionBody = "\nint status = 0;\n"

        arguments = ""
        for p in rawParamStructs:
            if p.name == returnName or p.name == discardName:
                arguments += "&"
            if p.isPtr and p.type=="void":
                arguments += "%sPtr + %sOffset, " % (p.name, p.name)
            elif p.isPtr and p.type=="char":
                arguments += "%sPtr, " % p.name
            else:
                arguments += p.name + ", "
        arguments = arguments[:-2]

        if returnName:
            if returnCategory == "intType" or returnType=="int":
                newFunctionBody += "jint %s = 0;\n" % returnName
            else:
                newFunctionBody += returnType +" %s;\n" % returnName
        if discardName:
            newFunctionBody += "jint %s = 0;\n" % discardName

        #If we have arrays, add the code to handle them
        arrayNames          = [p.name for p in newParamStructs if p.isPtr]
        charArrayNames      = [p.name for p in newParamStructs if p.isPtr and p.type=="char"]
        voidArrayNames      = [p.name for p in newParamStructs if p.isPtr and p.type=="void"]
        outVoidArrayNames   = [p.name for p in newParamStructs if p.isPtr and p.type=="void" and p.isOut]
        if arrayNames:
            #Declare C pointers to retrieve array contents
            for n in arrayNames:
                newFunctionBody += "jbyte* " + n + "Ptr = 0;\n"
            newFunctionBody += "\n"
            #Retrieve the contents for strings
            #We need to do this first cause in one case this char* is needed as an argument
            if charArrayNames:
                for n in charArrayNames:
                    newFunctionBody += "if (!getPointerString(env, %(n)s, &%(n)sPtr))\n\tgoto finish;\n" % vars()
                newFunctionBody += "\n"
            #Query the length of output data
            #CryptPopData is the only exception that produces output data
            #but doesn't require this cause an explicit length is passed in
            if outVoidArrayNames and functionName.find("PopData") == -1:
                for n in outVoidArrayNames:
                    argumentsWithNull = arguments.replace("%sPtr + %sOffset" % (n,n), "NULL")
                    newFunctionBody += "if (!processStatus(env, crypt%s(%s)))\n\tgoto finish;\n" % (functionName, argumentsWithNull)
                newFunctionBody += "\n"
            elif functionName.find("PopData") != -1:
                newFunctionBody += "//CryptPopData is a special case that doesn't have the length querying call\n\n"
            if voidArrayNames:
                for n in voidArrayNames:
                    index = [p.name for p in newParamStructs].index(n)
                    if n in outVoidArrayNames:
                        lengthName = returnName
                    else:
                        if len(newParamStructs)<=index+2 or newParamStructs[index+2].category != "rawType" or newParamStructs[index+2].type != "int":
                            lengthName = "1" #CheckSignature and ImportKey don't have a length, so we can't do a good check!
                        else:
                            lengthName = newParamStructs[index+2].name
                    newFunctionBody += "if (!checkIndices%(voidTag)s(env, %(n)s, %(n)sOffset, %(lengthName)s))\n\tgoto finish;\n" % vars()
                newFunctionBody += "\n"
            for n in voidArrayNames:
                newFunctionBody += "if (!getPointer%(voidTag)s(env, %(n)s, &%(n)sPtr))\n\tgoto finish;\n" % vars()

        if newFunctionBody[-2] != "\n":
            newFunctionBody += "\n"
        newFunctionBody += "status = crypt%s(%s);\n\n" % (functionName, arguments)

        if arrayNames:
            newFunctionBody += "finish:\n"
        if arrayNames:
            for n in voidArrayNames:
                newFunctionBody += "releasePointer%(voidTag)s(env, %(n)s, %(n)sPtr);\n" % vars()
            for n in charArrayNames:
                newFunctionBody += "releasePointerString(env, %(n)s, %(n)sPtr);\n" % vars()

        #newFunctionBody += "processStatus(env, status);\n"

        #if returnName:
        #    newFunctionBody += "return(%s);\n" % returnName
        if returnName:
            if returnCategory == "intType" or returnType == "int":
                newFunctionBody += "processStatus(env, status);\n"
                newFunctionBody += "return(%s);\n" % returnName
            elif returnType == "CRYPT_QUERY_INFO":
                newFunctionBody += "return(processStatusReturnCryptQueryInfo(env, status, %s));\n" % returnName
            elif returnType == "CRYPT_OBJECT_INFO":
                newFunctionBody += "return(processStatusReturnCryptObjectInfo(env, status, %s));\n" % returnName
        else:
            newFunctionBody += "processStatus(env, status);\n"

        newFunctionBody = newFunctionBody[:-1] #Strip off last \n
        newFunctionBody = newFunctionBody.replace("\n", "\n\t");
        newFunction = function[:-1]+"\n{" + newFunctionBody + "\n}"

        #Substitute the output equivalent for the input
        s = s[ : functionMatch.start()] + newFunction + s[functionMatch.end() : ]

        #Get next function
        functionMatch = functionPattern.search(s, functionMatch.start() + len(newFunction))

    f = open("java_jni.c", "w")
    f.write(s)
    f.close()



elif language == "python":
    os.chdir(outDir)

    #print sInts
    #raw_input()
    #print sFuncs
    #raw_input()

    moduleFunctions = ""
    s =         pyBeforeExceptions + exceptionString + pyBeforeFuncs + sFuncs + \
                pyBeforeModuleFunctions + moduleFunctions + sModFuncs + \
                pyBeforeInts + sInts + \
                pyAfterInts

    #print s
    #raw_input()
    functionPattern = re.compile(r"static PyObject\* python_crypt([^\(]+)[^{]+{([^}]*)}", re.DOTALL)
    functionMatch = functionPattern.search(s)
    while functionMatch:
        #print functionMatch.group()
        #print functionMatch.groups()
        #raw_input()

        #Most of the code in this loop is copied-then-modified from the java code above, ugly..
        voidTag = ""

        #Extract the function name and body
        functionName, functionBody = functionMatch.groups()

        #Look up the parameters we've previously determined for this function
        rawParamStructs = rawParamStructsDict[functionName]
        newParamStructs = newParamStructsDict[functionName]
        lengthIndices = lengthIndicesDict[functionName]
        offsetIndices = offsetIndicesDict[functionName]
        #print functionName, lengthIndices, offsetIndices
        #raw_input()
        if newReturnStructsDict.get(functionName):
            returnName = newReturnStructsDict.get(functionName).name
            returnType = newReturnStructsDict.get(functionName).type
            returnCategory = newReturnStructsDict.get(functionName).category
        else:
            returnName = None
        if newDiscardedStructsDict.get(functionName):
            discardName = newDiscardedStructsDict.get(functionName).name
        else:
            discardName = None

        newFunctionBody = "\nint status = 0;\n"

        #Arguments to pass to the C cryptlib function
        arguments = ""
        for p in rawParamStructs:
            if p.name == returnName or p.name == discardName:
                arguments += "&"
            if p.isPtr and p.type=="void":
                arguments += "%sPtr, " % (p.name)
            elif p.isPtr and p.type=="char":
                arguments += "%sPtr, " % p.name
            else:
                arguments += p.name + ", "
        arguments = arguments[:-2]

        if returnName:
            if returnCategory == "intType" or returnType=="int":
                newFunctionBody += "int %s = 0;\n" % returnName
            else:
                newFunctionBody += returnType +" %s;\n" % returnName
        if discardName:
            newFunctionBody += "int %s = 0;\n" % discardName
        #Declare variables to parse args tuple into
        index = 0
        for p in newParamStructs:
            if index not in offsetIndices:       #Python doesn't use the offsets
                if p.isPtr:
                    newFunctionBody += "PyObject* %s = NULL;\n" % p.name
                else:
                    newFunctionBody += "int %s = 0;\n" % p.name
            index += 1

        #If we have arrays, add the code to handle them
        arrayNames          = [p.name for p in newParamStructs if p.isPtr]
        charArrayNames      = [p.name for p in newParamStructs if p.isPtr and p.type=="char"]
        voidArrayNames      = [p.name for p in newParamStructs if p.isPtr and p.type=="void"]
        outVoidArrayNames   = [p.name for p in newParamStructs if p.isPtr and p.type=="void" and p.isOut]
        if arrayNames:
            #Declare C pointers to retrieve array contents
            for n in arrayNames:
                newFunctionBody += "unsigned char* " + n + "Ptr = 0;\n"
            newFunctionBody += "\n"
            #Retrieve the contents for strings
            #We need to do this first cause in one case this char* is needed as an argument

        #Parse the input arguments from the python user
        #Arguments to parse from the python args tuple
        if newParamStructs:
            parseFormatString = ""
            parseArgsList = []
            index = 0
            for p in newParamStructs:
                if index not in lengthIndices and index not in offsetIndices:
                    if p.isPtr and p.type=="char":
                        parseFormatString += "O"
                        parseArgsList.append("&" + p.name)
                    elif p.isPtr and p.type=="void":
                        parseFormatString += "O"
                        parseArgsList.append("&" + p.name)
                    else:
                        parseFormatString += "i"
                        parseArgsList.append("&" + p.name)

                index += 1

            if newFunctionBody[-2] != "\n":
                newFunctionBody += "\n"


            if functionName == "AddRandom":
                newFunctionBody += """\
//Special case to handle SLOWPOLL / FASTPOLL values
if (PyArg_ParseTuple(args, "i", &randomDataLength))
    return processStatus(cryptAddRandom(NULL, randomDataLength));\n\n"""

            newFunctionBody += """\
if (!PyArg_ParseTuple(args, "%s", %s))
    return(NULL);\n\n""" % (parseFormatString, ", ".join(parseArgsList))
            #for p in newParamStructs:
            #    print p.name,

        if arrayNames:
            if charArrayNames:
                for n in charArrayNames:
                    newFunctionBody += "if (!getPointerReadString(%(n)s, &%(n)sPtr))\n\tgoto finish;\n" % vars()
                newFunctionBody += "\n"
            #Query the length of output data
            #CryptPopData is the only exception that produces output data
            #but doesn't require this cause an explicit length is passed in
            if outVoidArrayNames and functionName.find("PopData") == -1:
                for n in outVoidArrayNames:
                    argumentsWithNull = arguments.replace("%sPtr" % (n), "NULL")
                    newFunctionBody += "if (!processStatusBool(crypt%s(%s)))\n\tgoto finish;\n" % (functionName, argumentsWithNull)
                newFunctionBody += "\n"
            elif functionName.find("PopData") != -1:
                newFunctionBody += "//CryptPopData is a special case that doesn't have the length querying call\n\n"

            #Go through and get the pointers for translated void*
            if voidArrayNames:
                for n in voidArrayNames:
                    index = [p.name for p in newParamStructs].index(n)
                    #Determine the name of its length parameter
                    if n in outVoidArrayNames:
                        lengthName = returnName
                    else:
                        if len(newParamStructs)<=index+2 or newParamStructs[index+2].category != "rawType" or newParamStructs[index+2].type != "int":
                            lengthName = "1" #QueryObject, CheckSignature and ImportKey don't have a length, so we can't do a good check!
                        else:
                            lengthName = newParamStructs[index+2].name
                    if n in outVoidArrayNames and functionName.find("PopData") == -1:
                        newFunctionBody += "if (!getPointerWriteCheckIndices%(voidTag)s(%(n)s, &%(n)sPtr, &%(lengthName)s))\n\tgoto finish;\n" % vars()
                    else:
                        if lengthName == "1": #Handle the #CheckSignature/ImportKey case
                            newFunctionBody += "if (!getPointerReadNoLength%(voidTag)s(%(n)s, &%(n)sPtr))\n\tgoto finish;\n" % vars()
                        else:
                            #If the pointer is C_IN and not C_INOUT
                            #(we check against Encrypt/Decrypt directory for this latter check)
                            #We only need it for reading, not writing
                            if n not in outVoidArrayNames and functionName not in ("Encrypt", "Decrypt"):
                                newFunctionBody += "if (!getPointerRead(%(n)s, &%(n)sPtr, &%(lengthName)s))\n\tgoto finish;\n" % vars()
                            else:
                                newFunctionBody += "if (!getPointerWrite(%(n)s, &%(n)sPtr, &%(lengthName)s))\n\tgoto finish;\n" % vars()

        if newFunctionBody[-2] != "\n":
            newFunctionBody += "\n"
        newFunctionBody += "status = crypt%s(%s);\n\n" % (functionName, arguments)

        if arrayNames:
            newFunctionBody += "finish:\n"
        if arrayNames:
            for n in voidArrayNames:
                newFunctionBody += "releasePointer(%(n)s, %(n)sPtr);\n" % vars()
            for n in charArrayNames:
                newFunctionBody += "releasePointerString(%(n)s, %(n)sPtr);\n" % vars()

        if returnName:
            if returnCategory == "intType":
                newFunctionBody += "return(processStatusReturnCryptHandle(status, %s));" % returnName
            elif returnType == "CRYPT_QUERY_INFO":
                newFunctionBody += "return(processStatusReturnCryptQueryInfo(status, %s));" % returnName
            elif returnType == "CRYPT_OBJECT_INFO":
                newFunctionBody += "return(processStatusReturnCryptObjectInfo(status, %s));" % returnName
            elif returnType == "int":
                newFunctionBody += "return(processStatusReturnInt(status, %s));" % returnName
        else:
            newFunctionBody += "return(processStatus(status));"

        newFunctionBody = newFunctionBody.replace("\n", "\n\t")

        #Substitute the output equivalent for the input
        s = s[ : functionMatch.start(2)] + newFunctionBody + "\n" + s[functionMatch.end(2) : ]

        #Get next function
        functionMatch = functionPattern.search(s, functionMatch.start() + len(newFunction))

    f = open("python.c", "w")
    f.write(s)
    f.close()


    f = open("setup.py", "w")
    f.write(setupPy)
    f.close()

elif language == "net":
    flagForAddRandomHack = 0

    #functionPattern = re.compile(r"public static ([^ \t]+) ([^ \t\n]+)\([ \t\n]*\(([^\)]*)\) throws CryptException;")
    functionPattern = re.compile(r"public static ([^ \t]+) ([^ \t\n]+)\([ \t\n]*([^\)]*)\);")
    functionMatch = functionPattern.search(s)
    while functionMatch:
        #print functionMatch.groups()

        #Extract the returnType, name, and prototype for this function
        function = functionMatch.group()
        functionReturnType, functionName, functionPrototype = functionMatch.groups()

        #Handle special-case AddRandom(pollType) function
        if functionName == "AddRandom":
            flagForAddRandomHack += 1
        if flagForAddRandomHack == 2:
            p = ParamStruct()
            p.type = "int"
            p.isPtr = 0
            p.isOut = 0
            p.category = "intType"
            p.name = "IntPtr.Zero"
            p.rawIndex = 0
            p2 = ParamStruct()
            p2.type = "int"
            p2.isPtr = 0
            p2.isOut = 0
            p2.category = "intType"
            p2.name = "pollType"
            p2.rawIndex = 1
            rawParamStructs = [p,p2]
            newParamStructs = [p2,p2]
            voidTag = "Array"
            functionName = functionName.split("__")[0]
            returnName = None
            discardName = None
            flagForAddRandomHack = None
        else:
            #Look up the parameters we've previously determined for this function
            rawParamStructs = rawParamStructsDict[functionName]
            newParamStructs = newParamStructsDict[functionName]
            if newReturnStructsDict.get(functionName):
                returnName = newReturnStructsDict.get(functionName).name
                returnType = newReturnStructsDict.get(functionName).type
                returnCategory = newReturnStructsDict.get(functionName).category
            else:
                returnName = None
            if newDiscardedStructsDict.get(functionName):
                discardName = newDiscardedStructsDict.get(functionName).name
            else:
                discardName = None

        #for p in newParamStructs: print "     "+str(p)

        #Substitute parameter names into the function prototype
        newFunctionParams = functionPrototype #expandFunctionPrototype(functionPrototype, newParamStructs)
        startIndex = functionMatch.start(3) - functionMatch.start()
        endIndex = functionMatch.end(3) - functionMatch.start()
        function = function[ : startIndex] + newFunctionParams + function[ endIndex : ]

        newFunctionBody = "\n"

        arguments = ""
        for p in rawParamStructs:
            if p.name == returnName or p.name == discardName:
                arguments += "%sPtr, " % p.name
            elif p.isPtr and p.type=="void":
                arguments += "%sPtr, " % p.name
            elif p.isPtr and p.type=="char":
                arguments += "%sPtr, " % p.name
            else:
                arguments += p.name + ", "
        arguments = arguments[:-2]

        if returnName:
            if returnCategory == "structType":
                newFunctionBody += "IntPtr %sPtr = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(%s)));\n" % (returnName, returnType)
                newFunctionBody += "%s %s = new %s();\n" % (returnType, returnName, returnType)
            else:
                newFunctionBody += "IntPtr %sPtr = Marshal.AllocHGlobal(4);\n" % returnName
        if discardName:
            newFunctionBody += "IntPtr %sPtr = Marshal.AllocHGlobal(4);\n" % discardName

        #If we have arrays, add the code to handle them
        arrayNames          = [p.name for p in newParamStructs if p.isPtr]
        charArrayNames      = [p.name for p in newParamStructs if p.isPtr and p.type=="char"]
        voidArrayNames      = [p.name for p in newParamStructs if p.isPtr and p.type=="void"]
        outVoidArrayNames   = [p.name for p in newParamStructs if p.isPtr and p.type=="void" and p.isOut]
        if arrayNames:
            #Declare C pointers to retrieve array contents
            for n in arrayNames:
                #newFunctionBody += "jbyte* " + n + "Ptr = 0;\n"
                newFunctionBody += "GCHandle %sHandle = new GCHandle();\nIntPtr %sPtr = IntPtr.Zero;\n" % (n,n)
                if n in charArrayNames:
                    newFunctionBody += "byte[] %sArray = new UTF8Encoding().GetBytes(%s);\n" % (n,n)

            newFunctionBody += "try\n{\n"

            #Retrieve the contents for strings
            #We need to do this first cause in one case this char* is needed as an argument
            if charArrayNames:
                for n in charArrayNames:
                    newFunctionBody += "getPointer(%(n)sArray, 0, ref %(n)sHandle, ref %(n)sPtr);\n" % vars()
            #Query the length of output data
            #CryptPopData is the only exception that produces output data
            #but doesn't require this cause an explicit length is passed in
            if outVoidArrayNames and functionName.find("PopData") == -1:
                for n in outVoidArrayNames:
                    argumentsWithNull = arguments.replace("%sPtr + %sOffset" % (n,n), "NULL")
                    newFunctionBody += "processStatus(wrapped_%s(%s));\n" % (functionName, argumentsWithNull)
                    newFunctionBody += "int %s = Marshal.ReadInt32(%sPtr);\n" % (returnName, returnName)
            elif functionName.find("PopData") != -1 or functionName.find("PushData") != -1:
                newFunctionBody += "int %s = 0;\n" % returnName
		newFunctionBody += "int status;\n"
            if voidArrayNames:
                for n in voidArrayNames:
                    index = [p.name for p in newParamStructs].index(n)
                    if n in outVoidArrayNames:
                        lengthName = returnName
                    else:
                        if len(newParamStructs)<=index+2 or newParamStructs[index+2].category != "rawType" or newParamStructs[index+2].type != "int":
                            lengthName = "1" #CheckSignature and ImportKey don't have a length, so we can't do a good check!
                        else:
                            lengthName = newParamStructs[index+2].name
                    newFunctionBody += "checkIndices(%(n)s, %(n)sOffset, %(lengthName)s);\n" % vars()
            for n in voidArrayNames:
                newFunctionBody += "getPointer(%(n)s, %(n)sOffset, ref %(n)sHandle, ref %(n)sPtr);\n" % vars()
        elif returnName:
            newFunctionBody += "try\n{\n"

	if functionName.find("PopData") == -1 and functionName.find("PushData") == -1:
		newFunctionBody += "processStatus(wrapped_%s(%s));\n" % (functionName, arguments)
	else:
		newFunctionBody += "status = wrapped_%s(%s);\n" % (functionName, arguments)
		newFunctionBody += "%s = Marshal.ReadInt32(%sPtr);\n" % (returnName, returnName)
		newFunctionBody += "processStatus(status, %s);\n" %returnName

        #if newFunctionBody[-2] != "\n":
        #    newFunctionBody += "\n"
        if returnName:
            if returnCategory == "structType":
                newFunctionBody += "Marshal.PtrToStructure(%sPtr, %s);\n" % (returnName, returnName)
                newFunctionBody += "return %s;\n" % returnName
            else:
				if functionName.find("PopData") == -1 and functionName.find("PushData") == -1:
					newFunctionBody += "return Marshal.ReadInt32(%sPtr);\n" % returnName
				else:
					newFunctionBody += "return %s;\n" %returnName

        if arrayNames or returnName:
            newFunctionBody += "}\nfinally\n{\n"
            if returnName:
                newFunctionBody += "Marshal.FreeHGlobal(%sPtr);\n" % returnName
            if arrayNames:
                for n in voidArrayNames:
                    newFunctionBody += "releasePointer(%(n)sHandle);\n" % vars()
                for n in charArrayNames:
                    newFunctionBody += "releasePointer(%(n)sHandle);\n" % vars()
            newFunctionBody += "}\n"

        #Add tabs to lines inside brackets:
        lines = newFunctionBody.split("\n")
        brackets = 0
        for count in range(len(lines)):
            line = lines[count]
            if line.startswith("}"):
                brackets -= 1
            lines[count] = ("\t" * brackets) + line
            if line.startswith("{"):
                brackets += 1
        newFunctionBody = "\n".join(lines)

        newFunctionBody = newFunctionBody[:-1] #Strip off last \n
        newFunctionBody = newFunctionBody.replace("\n", "\n\t");
        newFunction = function[:-1]+"\n{" + newFunctionBody + "\n}"
        newFunction = newFunction.replace("\n", "\n\t");

        #Substitute the output equivalent for the input
        s = s[ : functionMatch.start()] + newFunction + s[functionMatch.end() : ]

        #Get next function
        functionMatch = functionPattern.search(s, functionMatch.start() + len(newFunction))

    #Add enclosing class syntax
    #---------------------------
    s = classPrefix + s

    for functionName in functionNames:
        rawParamStructs = rawParamStructsDict[functionName]
        parameters = ""
        for p in rawParamStructs:
            if p.isPtr:
                parameters += "IntPtr %s, " % p.name
            else:
                parameters += "int %s, " % p.name
        parameters = parameters[:-2]

        s += '\t[DllImport("cl32.dll", EntryPoint="crypt%s")]\n\tprivate static extern int wrapped_%s(%s);\n\n' % (functionName, functionName, parameters)
        functionNames = [] #Accumulate function names here
        #rawParamStructsDict = {} #Accumulate function name -> list of ParamStructs (of original c code)
        #newParamStructsDict = {} #Accumulate function name -> list of ParamStructs (of new java/python code)



    s += classPostfix + exceptionString + "\n\n}"

    os.chdir(outDir)
    print "Writing .NET file..."
    f = open("cryptlib.cs", "w")
    f.write(s)
    f.close()


