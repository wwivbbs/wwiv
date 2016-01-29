
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
    else if (status == CRYPT_ERROR_PARAM1)
        o = Py_BuildValue("(is)", CRYPT_ERROR_PARAM1, "Bad argument, parameter 1");
    else if (status == CRYPT_ERROR_PARAM2)
        o = Py_BuildValue("(is)", CRYPT_ERROR_PARAM2, "Bad argument, parameter 2");
    else if (status == CRYPT_ERROR_PARAM3)
        o = Py_BuildValue("(is)", CRYPT_ERROR_PARAM3, "Bad argument, parameter 3");
    else if (status == CRYPT_ERROR_PARAM4)
        o = Py_BuildValue("(is)", CRYPT_ERROR_PARAM4, "Bad argument, parameter 4");
    else if (status == CRYPT_ERROR_PARAM5)
        o = Py_BuildValue("(is)", CRYPT_ERROR_PARAM5, "Bad argument, parameter 5");
    else if (status == CRYPT_ERROR_PARAM6)
        o = Py_BuildValue("(is)", CRYPT_ERROR_PARAM6, "Bad argument, parameter 6");
    else if (status == CRYPT_ERROR_PARAM7)
        o = Py_BuildValue("(is)", CRYPT_ERROR_PARAM7, "Bad argument, parameter 7");
    else if (status == CRYPT_ERROR_MEMORY)
        o = Py_BuildValue("(is)", CRYPT_ERROR_MEMORY, "Out of memory");
    else if (status == CRYPT_ERROR_NOTINITED)
        o = Py_BuildValue("(is)", CRYPT_ERROR_NOTINITED, "Data has not been initialised");
    else if (status == CRYPT_ERROR_INITED)
        o = Py_BuildValue("(is)", CRYPT_ERROR_INITED, "Data has already been init'd");
    else if (status == CRYPT_ERROR_NOSECURE)
        o = Py_BuildValue("(is)", CRYPT_ERROR_NOSECURE, "Opn.not avail.at requested sec.level");
    else if (status == CRYPT_ERROR_RANDOM)
        o = Py_BuildValue("(is)", CRYPT_ERROR_RANDOM, "No reliable random data available");
    else if (status == CRYPT_ERROR_FAILED)
        o = Py_BuildValue("(is)", CRYPT_ERROR_FAILED, "Operation failed");
    else if (status == CRYPT_ERROR_INTERNAL)
        o = Py_BuildValue("(is)", CRYPT_ERROR_INTERNAL, "Internal consistency check failed");
    else if (status == CRYPT_ERROR_NOTAVAIL)
        o = Py_BuildValue("(is)", CRYPT_ERROR_NOTAVAIL, "This type of opn.not available");
    else if (status == CRYPT_ERROR_PERMISSION)
        o = Py_BuildValue("(is)", CRYPT_ERROR_PERMISSION, "No permiss.to perform this operation");
    else if (status == CRYPT_ERROR_WRONGKEY)
        o = Py_BuildValue("(is)", CRYPT_ERROR_WRONGKEY, "Incorrect key used to decrypt data");
    else if (status == CRYPT_ERROR_INCOMPLETE)
        o = Py_BuildValue("(is)", CRYPT_ERROR_INCOMPLETE, "Operation incomplete/still in progress");
    else if (status == CRYPT_ERROR_COMPLETE)
        o = Py_BuildValue("(is)", CRYPT_ERROR_COMPLETE, "Operation complete/can't continue");
    else if (status == CRYPT_ERROR_TIMEOUT)
        o = Py_BuildValue("(is)", CRYPT_ERROR_TIMEOUT, "Operation timed out before completion");
    else if (status == CRYPT_ERROR_INVALID)
        o = Py_BuildValue("(is)", CRYPT_ERROR_INVALID, "Invalid/inconsistent information");
    else if (status == CRYPT_ERROR_SIGNALLED)
        o = Py_BuildValue("(is)", CRYPT_ERROR_SIGNALLED, "Resource destroyed by extnl.event");
    else if (status == CRYPT_ERROR_OVERFLOW)
        o = Py_BuildValue("(is)", CRYPT_ERROR_OVERFLOW, "Resources/space exhausted");
    else if (status == CRYPT_ERROR_UNDERFLOW)
        o = Py_BuildValue("(is)", CRYPT_ERROR_UNDERFLOW, "Not enough data available");
    else if (status == CRYPT_ERROR_BADDATA)
        o = Py_BuildValue("(is)", CRYPT_ERROR_BADDATA, "Bad/unrecognised data format");
    else if (status == CRYPT_ERROR_SIGNATURE)
        o = Py_BuildValue("(is)", CRYPT_ERROR_SIGNATURE, "Signature/integrity check failed");
    else if (status == CRYPT_ERROR_OPEN)
        o = Py_BuildValue("(is)", CRYPT_ERROR_OPEN, "Cannot open object");
    else if (status == CRYPT_ERROR_READ)
        o = Py_BuildValue("(is)", CRYPT_ERROR_READ, "Cannot read item from object");
    else if (status == CRYPT_ERROR_WRITE)
        o = Py_BuildValue("(is)", CRYPT_ERROR_WRITE, "Cannot write item to object");
    else if (status == CRYPT_ERROR_NOTFOUND)
        o = Py_BuildValue("(is)", CRYPT_ERROR_NOTFOUND, "Requested item not found in object");
    else if (status == CRYPT_ERROR_DUPLICATE)
        o = Py_BuildValue("(is)", CRYPT_ERROR_DUPLICATE, "Item already present in object");
    else if (status == CRYPT_ENVELOPE_RESOURCE)
        o = Py_BuildValue("(is)", CRYPT_ENVELOPE_RESOURCE, "Need resource to proceed");
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

static PyObject* python_cryptInit(PyObject* self, PyObject* args)
{
	int status = 0;
	
	status = cryptInit();
	
	return(processStatus(status));
}

static PyObject* python_cryptEnd(PyObject* self, PyObject* args)
{
	int status = 0;
	
	status = cryptEnd();
	
	return(processStatus(status));
}

static PyObject* python_cryptQueryCapability(PyObject* self, PyObject* args)
{
	int status = 0;
	CRYPT_QUERY_INFO cryptQueryInfo;
	int cryptAlgo = 0;
	
	if (!PyArg_ParseTuple(args, "i", &cryptAlgo))
	    return(NULL);
	
	status = cryptQueryCapability(cryptAlgo, &cryptQueryInfo);
	
	return(processStatusReturnCryptQueryInfo(status, cryptQueryInfo));
}

static PyObject* python_cryptCreateContext(PyObject* self, PyObject* args)
{
	int status = 0;
	int cryptContext = 0;
	int cryptUser = 0;
	int cryptAlgo = 0;
	
	if (!PyArg_ParseTuple(args, "ii", &cryptUser, &cryptAlgo))
	    return(NULL);
	
	status = cryptCreateContext(&cryptContext, cryptUser, cryptAlgo);
	
	return(processStatusReturnCryptHandle(status, cryptContext));
}

static PyObject* python_cryptDestroyContext(PyObject* self, PyObject* args)
{
	int status = 0;
	int cryptContext = 0;
	
	if (!PyArg_ParseTuple(args, "i", &cryptContext))
	    return(NULL);
	
	status = cryptDestroyContext(cryptContext);
	
	return(processStatus(status));
}

static PyObject* python_cryptDestroyObject(PyObject* self, PyObject* args)
{
	int status = 0;
	int cryptObject = 0;
	
	if (!PyArg_ParseTuple(args, "i", &cryptObject))
	    return(NULL);
	
	status = cryptDestroyObject(cryptObject);
	
	return(processStatus(status));
}

static PyObject* python_cryptGenerateKey(PyObject* self, PyObject* args)
{
	int status = 0;
	int cryptContext = 0;
	
	if (!PyArg_ParseTuple(args, "i", &cryptContext))
	    return(NULL);
	
	status = cryptGenerateKey(cryptContext);
	
	return(processStatus(status));
}

static PyObject* python_cryptEncrypt(PyObject* self, PyObject* args)
{
	int status = 0;
	int cryptContext = 0;
	PyObject* buffer = NULL;
	int length = 0;
	unsigned char* bufferPtr = 0;
	
	if (!PyArg_ParseTuple(args, "iO", &cryptContext, &buffer))
	    return(NULL);
	
	if (!getPointerWrite(buffer, &bufferPtr, &length))
		goto finish;
	
	status = cryptEncrypt(cryptContext, bufferPtr, length);
	
	finish:
	releasePointer(buffer, bufferPtr);
	return(processStatus(status));
}

static PyObject* python_cryptDecrypt(PyObject* self, PyObject* args)
{
	int status = 0;
	int cryptContext = 0;
	PyObject* buffer = NULL;
	int length = 0;
	unsigned char* bufferPtr = 0;
	
	if (!PyArg_ParseTuple(args, "iO", &cryptContext, &buffer))
	    return(NULL);
	
	if (!getPointerWrite(buffer, &bufferPtr, &length))
		goto finish;
	
	status = cryptDecrypt(cryptContext, bufferPtr, length);
	
	finish:
	releasePointer(buffer, bufferPtr);
	return(processStatus(status));
}

static PyObject* python_cryptSetAttribute(PyObject* self, PyObject* args)
{
	int status = 0;
	int cryptHandle = 0;
	int attributeType = 0;
	int value = 0;
	
	if (!PyArg_ParseTuple(args, "iii", &cryptHandle, &attributeType, &value))
	    return(NULL);
	
	status = cryptSetAttribute(cryptHandle, attributeType, value);
	
	return(processStatus(status));
}

static PyObject* python_cryptSetAttributeString(PyObject* self, PyObject* args)
{
	int status = 0;
	int cryptHandle = 0;
	int attributeType = 0;
	PyObject* value = NULL;
	int valueLength = 0;
	unsigned char* valuePtr = 0;
	
	if (!PyArg_ParseTuple(args, "iiO", &cryptHandle, &attributeType, &value))
	    return(NULL);
	
	if (!getPointerRead(value, &valuePtr, &valueLength))
		goto finish;
	
	status = cryptSetAttributeString(cryptHandle, attributeType, valuePtr, valueLength);
	
	finish:
	releasePointer(value, valuePtr);
	return(processStatus(status));
}

static PyObject* python_cryptGetAttribute(PyObject* self, PyObject* args)
{
	int status = 0;
	int value = 0;
	int cryptHandle = 0;
	int attributeType = 0;
	
	if (!PyArg_ParseTuple(args, "ii", &cryptHandle, &attributeType))
	    return(NULL);
	
	status = cryptGetAttribute(cryptHandle, attributeType, &value);
	
	return(processStatusReturnInt(status, value));
}

static PyObject* python_cryptGetAttributeString(PyObject* self, PyObject* args)
{
	int status = 0;
	int valueLength = 0;
	int cryptHandle = 0;
	int attributeType = 0;
	PyObject* value = NULL;
	unsigned char* valuePtr = 0;
	
	if (!PyArg_ParseTuple(args, "iiO", &cryptHandle, &attributeType, &value))
	    return(NULL);
	
	if (!processStatusBool(cryptGetAttributeString(cryptHandle, attributeType, NULL, &valueLength)))
		goto finish;
	
	if (!getPointerWriteCheckIndices(value, &valuePtr, &valueLength))
		goto finish;
	
	status = cryptGetAttributeString(cryptHandle, attributeType, valuePtr, &valueLength);
	
	finish:
	releasePointer(value, valuePtr);
	return(processStatusReturnInt(status, valueLength));
}

static PyObject* python_cryptDeleteAttribute(PyObject* self, PyObject* args)
{
	int status = 0;
	int cryptHandle = 0;
	int attributeType = 0;
	
	if (!PyArg_ParseTuple(args, "ii", &cryptHandle, &attributeType))
	    return(NULL);
	
	status = cryptDeleteAttribute(cryptHandle, attributeType);
	
	return(processStatus(status));
}

static PyObject* python_cryptAddRandom(PyObject* self, PyObject* args)
{
	int status = 0;
	PyObject* randomData = NULL;
	int randomDataLength = 0;
	unsigned char* randomDataPtr = 0;
	
	//Special case to handle SLOWPOLL / FASTPOLL values
	if (PyArg_ParseTuple(args, "i", &randomDataLength))
	    return processStatus(cryptAddRandom(NULL, randomDataLength));
	
	if (!PyArg_ParseTuple(args, "O", &randomData))
	    return(NULL);
	
	if (!getPointerRead(randomData, &randomDataPtr, &randomDataLength))
		goto finish;
	
	status = cryptAddRandom(randomDataPtr, randomDataLength);
	
	finish:
	releasePointer(randomData, randomDataPtr);
	return(processStatus(status));
}

static PyObject* python_cryptQueryObject(PyObject* self, PyObject* args)
{
	int status = 0;
	CRYPT_OBJECT_INFO cryptObjectInfo;
	PyObject* objectData = NULL;
	int objectDataLength = 0;
	unsigned char* objectDataPtr = 0;
	
	if (!PyArg_ParseTuple(args, "O", &objectData))
	    return(NULL);
	
	if (!getPointerRead(objectData, &objectDataPtr, &objectDataLength))
		goto finish;
	
	status = cryptQueryObject(objectDataPtr, objectDataLength, &cryptObjectInfo);
	
	finish:
	releasePointer(objectData, objectDataPtr);
	return(processStatusReturnCryptObjectInfo(status, cryptObjectInfo));
}

static PyObject* python_cryptExportKey(PyObject* self, PyObject* args)
{
	int status = 0;
	int encryptedKeyLength = 0;
	PyObject* encryptedKey = NULL;
	int encryptedKeyMaxLength = 0;
	int exportKey = 0;
	int sessionKeyContext = 0;
	unsigned char* encryptedKeyPtr = 0;
	
	if (!PyArg_ParseTuple(args, "Oiii", &encryptedKey, &encryptedKeyMaxLength, &exportKey, &sessionKeyContext))
	    return(NULL);
	
	if (!processStatusBool(cryptExportKey(NULL, encryptedKeyMaxLength, &encryptedKeyLength, exportKey, sessionKeyContext)))
		goto finish;
	
	if (!getPointerWriteCheckIndices(encryptedKey, &encryptedKeyPtr, &encryptedKeyLength))
		goto finish;
	
	status = cryptExportKey(encryptedKeyPtr, encryptedKeyMaxLength, &encryptedKeyLength, exportKey, sessionKeyContext);
	
	finish:
	releasePointer(encryptedKey, encryptedKeyPtr);
	return(processStatusReturnInt(status, encryptedKeyLength));
}

static PyObject* python_cryptExportKeyEx(PyObject* self, PyObject* args)
{
	int status = 0;
	int encryptedKeyLength = 0;
	PyObject* encryptedKey = NULL;
	int encryptedKeyMaxLength = 0;
	int formatType = 0;
	int exportKey = 0;
	int sessionKeyContext = 0;
	unsigned char* encryptedKeyPtr = 0;
	
	if (!PyArg_ParseTuple(args, "Oiiii", &encryptedKey, &encryptedKeyMaxLength, &formatType, &exportKey, &sessionKeyContext))
	    return(NULL);
	
	if (!processStatusBool(cryptExportKeyEx(NULL, encryptedKeyMaxLength, &encryptedKeyLength, formatType, exportKey, sessionKeyContext)))
		goto finish;
	
	if (!getPointerWriteCheckIndices(encryptedKey, &encryptedKeyPtr, &encryptedKeyLength))
		goto finish;
	
	status = cryptExportKeyEx(encryptedKeyPtr, encryptedKeyMaxLength, &encryptedKeyLength, formatType, exportKey, sessionKeyContext);
	
	finish:
	releasePointer(encryptedKey, encryptedKeyPtr);
	return(processStatusReturnInt(status, encryptedKeyLength));
}

static PyObject* python_cryptImportKey(PyObject* self, PyObject* args)
{
	int status = 0;
	PyObject* encryptedKey = NULL;
	int encryptedKeyLength = 0;
	int importKey = 0;
	int sessionKeyContext = 0;
	unsigned char* encryptedKeyPtr = 0;
	
	if (!PyArg_ParseTuple(args, "Oii", &encryptedKey, &importKey, &sessionKeyContext))
	    return(NULL);
	
	if (!getPointerRead(encryptedKey, &encryptedKeyPtr, &encryptedKeyLength))
		goto finish;
	
	status = cryptImportKey(encryptedKeyPtr, encryptedKeyLength, importKey, sessionKeyContext);
	
	finish:
	releasePointer(encryptedKey, encryptedKeyPtr);
	return(processStatus(status));
}

static PyObject* python_cryptImportKeyEx(PyObject* self, PyObject* args)
{
	int status = 0;
	int returnedContext = 0;
	PyObject* encryptedKey = NULL;
	int encryptedKeyLength = 0;
	int importKey = 0;
	int sessionKeyContext = 0;
	unsigned char* encryptedKeyPtr = 0;
	
	if (!PyArg_ParseTuple(args, "Oii", &encryptedKey, &importKey, &sessionKeyContext))
	    return(NULL);
	
	if (!getPointerRead(encryptedKey, &encryptedKeyPtr, &encryptedKeyLength))
		goto finish;
	
	status = cryptImportKeyEx(encryptedKeyPtr, encryptedKeyLength, importKey, sessionKeyContext, &returnedContext);
	
	finish:
	releasePointer(encryptedKey, encryptedKeyPtr);
	return(processStatusReturnCryptHandle(status, returnedContext));
}

static PyObject* python_cryptCreateSignature(PyObject* self, PyObject* args)
{
	int status = 0;
	int signatureLength = 0;
	PyObject* signature = NULL;
	int signatureMaxLength = 0;
	int signContext = 0;
	int hashContext = 0;
	unsigned char* signaturePtr = 0;
	
	if (!PyArg_ParseTuple(args, "Oiii", &signature, &signatureMaxLength, &signContext, &hashContext))
	    return(NULL);
	
	if (!processStatusBool(cryptCreateSignature(NULL, signatureMaxLength, &signatureLength, signContext, hashContext)))
		goto finish;
	
	if (!getPointerWriteCheckIndices(signature, &signaturePtr, &signatureLength))
		goto finish;
	
	status = cryptCreateSignature(signaturePtr, signatureMaxLength, &signatureLength, signContext, hashContext);
	
	finish:
	releasePointer(signature, signaturePtr);
	return(processStatusReturnInt(status, signatureLength));
}

static PyObject* python_cryptCreateSignatureEx(PyObject* self, PyObject* args)
{
	int status = 0;
	int signatureLength = 0;
	PyObject* signature = NULL;
	int signatureMaxLength = 0;
	int formatType = 0;
	int signContext = 0;
	int hashContext = 0;
	int extraData = 0;
	unsigned char* signaturePtr = 0;
	
	if (!PyArg_ParseTuple(args, "Oiiiii", &signature, &signatureMaxLength, &formatType, &signContext, &hashContext, &extraData))
	    return(NULL);
	
	if (!processStatusBool(cryptCreateSignatureEx(NULL, signatureMaxLength, &signatureLength, formatType, signContext, hashContext, extraData)))
		goto finish;
	
	if (!getPointerWriteCheckIndices(signature, &signaturePtr, &signatureLength))
		goto finish;
	
	status = cryptCreateSignatureEx(signaturePtr, signatureMaxLength, &signatureLength, formatType, signContext, hashContext, extraData);
	
	finish:
	releasePointer(signature, signaturePtr);
	return(processStatusReturnInt(status, signatureLength));
}

static PyObject* python_cryptCheckSignature(PyObject* self, PyObject* args)
{
	int status = 0;
	PyObject* signature = NULL;
	int signatureLength = 0;
	int sigCheckKey = 0;
	int hashContext = 0;
	unsigned char* signaturePtr = 0;
	
	if (!PyArg_ParseTuple(args, "Oii", &signature, &sigCheckKey, &hashContext))
	    return(NULL);
	
	if (!getPointerRead(signature, &signaturePtr, &signatureLength))
		goto finish;
	
	status = cryptCheckSignature(signaturePtr, signatureLength, sigCheckKey, hashContext);
	
	finish:
	releasePointer(signature, signaturePtr);
	return(processStatus(status));
}

static PyObject* python_cryptCheckSignatureEx(PyObject* self, PyObject* args)
{
	int status = 0;
	int extraData = 0;
	PyObject* signature = NULL;
	int signatureLength = 0;
	int sigCheckKey = 0;
	int hashContext = 0;
	unsigned char* signaturePtr = 0;
	
	if (!PyArg_ParseTuple(args, "Oii", &signature, &sigCheckKey, &hashContext))
	    return(NULL);
	
	if (!getPointerRead(signature, &signaturePtr, &signatureLength))
		goto finish;
	
	status = cryptCheckSignatureEx(signaturePtr, signatureLength, sigCheckKey, hashContext, &extraData);
	
	finish:
	releasePointer(signature, signaturePtr);
	return(processStatusReturnCryptHandle(status, extraData));
}

static PyObject* python_cryptKeysetOpen(PyObject* self, PyObject* args)
{
	int status = 0;
	int keyset = 0;
	int cryptUser = 0;
	int keysetType = 0;
	PyObject* name = NULL;
	int options = 0;
	unsigned char* namePtr = 0;
	
	if (!PyArg_ParseTuple(args, "iiOi", &cryptUser, &keysetType, &name, &options))
	    return(NULL);
	
	if (!getPointerReadString(name, &namePtr))
		goto finish;
	
	status = cryptKeysetOpen(&keyset, cryptUser, keysetType, namePtr, options);
	
	finish:
	releasePointerString(name, namePtr);
	return(processStatusReturnCryptHandle(status, keyset));
}

static PyObject* python_cryptKeysetClose(PyObject* self, PyObject* args)
{
	int status = 0;
	int keyset = 0;
	
	if (!PyArg_ParseTuple(args, "i", &keyset))
	    return(NULL);
	
	status = cryptKeysetClose(keyset);
	
	return(processStatus(status));
}

static PyObject* python_cryptGetPublicKey(PyObject* self, PyObject* args)
{
	int status = 0;
	int cryptContext = 0;
	int keyset = 0;
	int keyIDtype = 0;
	PyObject* keyID = NULL;
	unsigned char* keyIDPtr = 0;
	
	if (!PyArg_ParseTuple(args, "iiO", &keyset, &keyIDtype, &keyID))
	    return(NULL);
	
	if (!getPointerReadString(keyID, &keyIDPtr))
		goto finish;
	
	status = cryptGetPublicKey(keyset, &cryptContext, keyIDtype, keyIDPtr);
	
	finish:
	releasePointerString(keyID, keyIDPtr);
	return(processStatusReturnCryptHandle(status, cryptContext));
}

static PyObject* python_cryptGetPrivateKey(PyObject* self, PyObject* args)
{
	int status = 0;
	int cryptContext = 0;
	int keyset = 0;
	int keyIDtype = 0;
	PyObject* keyID = NULL;
	PyObject* password = NULL;
	unsigned char* keyIDPtr = 0;
	unsigned char* passwordPtr = 0;
	
	if (!PyArg_ParseTuple(args, "iiOO", &keyset, &keyIDtype, &keyID, &password))
	    return(NULL);
	
	if (!getPointerReadString(keyID, &keyIDPtr))
		goto finish;
	if (!getPointerReadString(password, &passwordPtr))
		goto finish;
	
	status = cryptGetPrivateKey(keyset, &cryptContext, keyIDtype, keyIDPtr, passwordPtr);
	
	finish:
	releasePointerString(keyID, keyIDPtr);
	releasePointerString(password, passwordPtr);
	return(processStatusReturnCryptHandle(status, cryptContext));
}

static PyObject* python_cryptGetKey(PyObject* self, PyObject* args)
{
	int status = 0;
	int cryptContext = 0;
	int keyset = 0;
	int keyIDtype = 0;
	PyObject* keyID = NULL;
	PyObject* password = NULL;
	unsigned char* keyIDPtr = 0;
	unsigned char* passwordPtr = 0;
	
	if (!PyArg_ParseTuple(args, "iiOO", &keyset, &keyIDtype, &keyID, &password))
	    return(NULL);
	
	if (!getPointerReadString(keyID, &keyIDPtr))
		goto finish;
	if (!getPointerReadString(password, &passwordPtr))
		goto finish;
	
	status = cryptGetKey(keyset, &cryptContext, keyIDtype, keyIDPtr, passwordPtr);
	
	finish:
	releasePointerString(keyID, keyIDPtr);
	releasePointerString(password, passwordPtr);
	return(processStatusReturnCryptHandle(status, cryptContext));
}

static PyObject* python_cryptAddPublicKey(PyObject* self, PyObject* args)
{
	int status = 0;
	int keyset = 0;
	int certificate = 0;
	
	if (!PyArg_ParseTuple(args, "ii", &keyset, &certificate))
	    return(NULL);
	
	status = cryptAddPublicKey(keyset, certificate);
	
	return(processStatus(status));
}

static PyObject* python_cryptAddPrivateKey(PyObject* self, PyObject* args)
{
	int status = 0;
	int keyset = 0;
	int cryptKey = 0;
	PyObject* password = NULL;
	unsigned char* passwordPtr = 0;
	
	if (!PyArg_ParseTuple(args, "iiO", &keyset, &cryptKey, &password))
	    return(NULL);
	
	if (!getPointerReadString(password, &passwordPtr))
		goto finish;
	
	status = cryptAddPrivateKey(keyset, cryptKey, passwordPtr);
	
	finish:
	releasePointerString(password, passwordPtr);
	return(processStatus(status));
}

static PyObject* python_cryptDeleteKey(PyObject* self, PyObject* args)
{
	int status = 0;
	int keyset = 0;
	int keyIDtype = 0;
	PyObject* keyID = NULL;
	unsigned char* keyIDPtr = 0;
	
	if (!PyArg_ParseTuple(args, "iiO", &keyset, &keyIDtype, &keyID))
	    return(NULL);
	
	if (!getPointerReadString(keyID, &keyIDPtr))
		goto finish;
	
	status = cryptDeleteKey(keyset, keyIDtype, keyIDPtr);
	
	finish:
	releasePointerString(keyID, keyIDPtr);
	return(processStatus(status));
}

static PyObject* python_cryptCreateCert(PyObject* self, PyObject* args)
{
	int status = 0;
	int certificate = 0;
	int cryptUser = 0;
	int certType = 0;
	
	if (!PyArg_ParseTuple(args, "ii", &cryptUser, &certType))
	    return(NULL);
	
	status = cryptCreateCert(&certificate, cryptUser, certType);
	
	return(processStatusReturnCryptHandle(status, certificate));
}

static PyObject* python_cryptDestroyCert(PyObject* self, PyObject* args)
{
	int status = 0;
	int certificate = 0;
	
	if (!PyArg_ParseTuple(args, "i", &certificate))
	    return(NULL);
	
	status = cryptDestroyCert(certificate);
	
	return(processStatus(status));
}

static PyObject* python_cryptGetCertExtension(PyObject* self, PyObject* args)
{
	int status = 0;
	int extensionLength = 0;
	int criticalFlag = 0;
	int certificate = 0;
	PyObject* oid = NULL;
	PyObject* extension = NULL;
	int extensionMaxLength = 0;
	unsigned char* oidPtr = 0;
	unsigned char* extensionPtr = 0;
	
	if (!PyArg_ParseTuple(args, "iOOi", &certificate, &oid, &extension, &extensionMaxLength))
	    return(NULL);
	
	if (!getPointerReadString(oid, &oidPtr))
		goto finish;
	
	if (!processStatusBool(cryptGetCertExtension(certificate, oidPtr, &criticalFlag, NULL, extensionMaxLength, &extensionLength)))
		goto finish;
	
	if (!getPointerWriteCheckIndices(extension, &extensionPtr, &extensionLength))
		goto finish;
	
	status = cryptGetCertExtension(certificate, oidPtr, &criticalFlag, extensionPtr, extensionMaxLength, &extensionLength);
	
	finish:
	releasePointer(extension, extensionPtr);
	releasePointerString(oid, oidPtr);
	return(processStatusReturnInt(status, extensionLength));
}

static PyObject* python_cryptAddCertExtension(PyObject* self, PyObject* args)
{
	int status = 0;
	int certificate = 0;
	PyObject* oid = NULL;
	int criticalFlag = 0;
	PyObject* extension = NULL;
	int extensionLength = 0;
	unsigned char* oidPtr = 0;
	unsigned char* extensionPtr = 0;
	
	if (!PyArg_ParseTuple(args, "iOiO", &certificate, &oid, &criticalFlag, &extension))
	    return(NULL);
	
	if (!getPointerReadString(oid, &oidPtr))
		goto finish;
	
	if (!getPointerRead(extension, &extensionPtr, &extensionLength))
		goto finish;
	
	status = cryptAddCertExtension(certificate, oidPtr, criticalFlag, extensionPtr, extensionLength);
	
	finish:
	releasePointer(extension, extensionPtr);
	releasePointerString(oid, oidPtr);
	return(processStatus(status));
}

static PyObject* python_cryptDeleteCertExtension(PyObject* self, PyObject* args)
{
	int status = 0;
	int certificate = 0;
	PyObject* oid = NULL;
	unsigned char* oidPtr = 0;
	
	if (!PyArg_ParseTuple(args, "iO", &certificate, &oid))
	    return(NULL);
	
	if (!getPointerReadString(oid, &oidPtr))
		goto finish;
	
	status = cryptDeleteCertExtension(certificate, oidPtr);
	
	finish:
	releasePointerString(oid, oidPtr);
	return(processStatus(status));
}

static PyObject* python_cryptSignCert(PyObject* self, PyObject* args)
{
	int status = 0;
	int certificate = 0;
	int signContext = 0;
	
	if (!PyArg_ParseTuple(args, "ii", &certificate, &signContext))
	    return(NULL);
	
	status = cryptSignCert(certificate, signContext);
	
	return(processStatus(status));
}

static PyObject* python_cryptCheckCert(PyObject* self, PyObject* args)
{
	int status = 0;
	int certificate = 0;
	int sigCheckKey = 0;
	
	if (!PyArg_ParseTuple(args, "ii", &certificate, &sigCheckKey))
	    return(NULL);
	
	status = cryptCheckCert(certificate, sigCheckKey);
	
	return(processStatus(status));
}

static PyObject* python_cryptImportCert(PyObject* self, PyObject* args)
{
	int status = 0;
	int certificate = 0;
	PyObject* certObject = NULL;
	int certObjectLength = 0;
	int cryptUser = 0;
	unsigned char* certObjectPtr = 0;
	
	if (!PyArg_ParseTuple(args, "Oi", &certObject, &cryptUser))
	    return(NULL);
	
	if (!getPointerRead(certObject, &certObjectPtr, &certObjectLength))
		goto finish;
	
	status = cryptImportCert(certObjectPtr, certObjectLength, cryptUser, &certificate);
	
	finish:
	releasePointer(certObject, certObjectPtr);
	return(processStatusReturnCryptHandle(status, certificate));
}

static PyObject* python_cryptExportCert(PyObject* self, PyObject* args)
{
	int status = 0;
	int certObjectLength = 0;
	PyObject* certObject = NULL;
	int certObjectMaxLength = 0;
	int certFormatType = 0;
	int certificate = 0;
	unsigned char* certObjectPtr = 0;
	
	if (!PyArg_ParseTuple(args, "Oiii", &certObject, &certObjectMaxLength, &certFormatType, &certificate))
	    return(NULL);
	
	if (!processStatusBool(cryptExportCert(NULL, certObjectMaxLength, &certObjectLength, certFormatType, certificate)))
		goto finish;
	
	if (!getPointerWriteCheckIndices(certObject, &certObjectPtr, &certObjectLength))
		goto finish;
	
	status = cryptExportCert(certObjectPtr, certObjectMaxLength, &certObjectLength, certFormatType, certificate);
	
	finish:
	releasePointer(certObject, certObjectPtr);
	return(processStatusReturnInt(status, certObjectLength));
}

static PyObject* python_cryptCAAddItem(PyObject* self, PyObject* args)
{
	int status = 0;
	int keyset = 0;
	int certificate = 0;
	
	if (!PyArg_ParseTuple(args, "ii", &keyset, &certificate))
	    return(NULL);
	
	status = cryptCAAddItem(keyset, certificate);
	
	return(processStatus(status));
}

static PyObject* python_cryptCAGetItem(PyObject* self, PyObject* args)
{
	int status = 0;
	int certificate = 0;
	int keyset = 0;
	int certType = 0;
	int keyIDtype = 0;
	PyObject* keyID = NULL;
	unsigned char* keyIDPtr = 0;
	
	if (!PyArg_ParseTuple(args, "iiiO", &keyset, &certType, &keyIDtype, &keyID))
	    return(NULL);
	
	if (!getPointerReadString(keyID, &keyIDPtr))
		goto finish;
	
	status = cryptCAGetItem(keyset, &certificate, certType, keyIDtype, keyIDPtr);
	
	finish:
	releasePointerString(keyID, keyIDPtr);
	return(processStatusReturnCryptHandle(status, certificate));
}

static PyObject* python_cryptCADeleteItem(PyObject* self, PyObject* args)
{
	int status = 0;
	int keyset = 0;
	int certType = 0;
	int keyIDtype = 0;
	PyObject* keyID = NULL;
	unsigned char* keyIDPtr = 0;
	
	if (!PyArg_ParseTuple(args, "iiiO", &keyset, &certType, &keyIDtype, &keyID))
	    return(NULL);
	
	if (!getPointerReadString(keyID, &keyIDPtr))
		goto finish;
	
	status = cryptCADeleteItem(keyset, certType, keyIDtype, keyIDPtr);
	
	finish:
	releasePointerString(keyID, keyIDPtr);
	return(processStatus(status));
}

static PyObject* python_cryptCACertManagement(PyObject* self, PyObject* args)
{
	int status = 0;
	int certificate = 0;
	int action = 0;
	int keyset = 0;
	int caKey = 0;
	int certRequest = 0;
	
	if (!PyArg_ParseTuple(args, "iiii", &action, &keyset, &caKey, &certRequest))
	    return(NULL);
	
	status = cryptCACertManagement(&certificate, action, keyset, caKey, certRequest);
	
	return(processStatusReturnCryptHandle(status, certificate));
}

static PyObject* python_cryptCreateEnvelope(PyObject* self, PyObject* args)
{
	int status = 0;
	int envelope = 0;
	int cryptUser = 0;
	int formatType = 0;
	
	if (!PyArg_ParseTuple(args, "ii", &cryptUser, &formatType))
	    return(NULL);
	
	status = cryptCreateEnvelope(&envelope, cryptUser, formatType);
	
	return(processStatusReturnCryptHandle(status, envelope));
}

static PyObject* python_cryptDestroyEnvelope(PyObject* self, PyObject* args)
{
	int status = 0;
	int envelope = 0;
	
	if (!PyArg_ParseTuple(args, "i", &envelope))
	    return(NULL);
	
	status = cryptDestroyEnvelope(envelope);
	
	return(processStatus(status));
}

static PyObject* python_cryptCreateSession(PyObject* self, PyObject* args)
{
	int status = 0;
	int session = 0;
	int cryptUser = 0;
	int formatType = 0;
	
	if (!PyArg_ParseTuple(args, "ii", &cryptUser, &formatType))
	    return(NULL);
	
	status = cryptCreateSession(&session, cryptUser, formatType);
	
	return(processStatusReturnCryptHandle(status, session));
}

static PyObject* python_cryptDestroySession(PyObject* self, PyObject* args)
{
	int status = 0;
	int session = 0;
	
	if (!PyArg_ParseTuple(args, "i", &session))
	    return(NULL);
	
	status = cryptDestroySession(session);
	
	return(processStatus(status));
}

static PyObject* python_cryptPushData(PyObject* self, PyObject* args)
{
	int status = 0;
	int bytesCopied = 0;
	int envelope = 0;
	PyObject* buffer = NULL;
	int length = 0;
	unsigned char* bufferPtr = 0;
	
	if (!PyArg_ParseTuple(args, "iO", &envelope, &buffer))
	    return(NULL);
	
	if (!getPointerRead(buffer, &bufferPtr, &length))
		goto finish;
	
	status = cryptPushData(envelope, bufferPtr, length, &bytesCopied);
	
	finish:
	releasePointer(buffer, bufferPtr);
	return(processStatusReturnInt(status, bytesCopied));
}

static PyObject* python_cryptFlushData(PyObject* self, PyObject* args)
{
	int status = 0;
	int envelope = 0;
	
	if (!PyArg_ParseTuple(args, "i", &envelope))
	    return(NULL);
	
	status = cryptFlushData(envelope);
	
	return(processStatus(status));
}

static PyObject* python_cryptPopData(PyObject* self, PyObject* args)
{
	int status = 0;
	int bytesCopied = 0;
	int envelope = 0;
	PyObject* buffer = NULL;
	int length = 0;
	unsigned char* bufferPtr = 0;
	
	if (!PyArg_ParseTuple(args, "iOi", &envelope, &buffer, &length))
	    return(NULL);
	
	//CryptPopData is a special case that doesn't have the length querying call
	
	if (!getPointerWrite(buffer, &bufferPtr, &bytesCopied))
		goto finish;
	
	status = cryptPopData(envelope, bufferPtr, length, &bytesCopied);
	
	finish:
	releasePointer(buffer, bufferPtr);
	return(processStatusReturnInt(status, bytesCopied));
}

static PyObject* python_cryptDeviceOpen(PyObject* self, PyObject* args)
{
	int status = 0;
	int device = 0;
	int cryptUser = 0;
	int deviceType = 0;
	PyObject* name = NULL;
	unsigned char* namePtr = 0;
	
	if (!PyArg_ParseTuple(args, "iiO", &cryptUser, &deviceType, &name))
	    return(NULL);
	
	if (!getPointerReadString(name, &namePtr))
		goto finish;
	
	status = cryptDeviceOpen(&device, cryptUser, deviceType, namePtr);
	
	finish:
	releasePointerString(name, namePtr);
	return(processStatusReturnCryptHandle(status, device));
}

static PyObject* python_cryptDeviceClose(PyObject* self, PyObject* args)
{
	int status = 0;
	int device = 0;
	
	if (!PyArg_ParseTuple(args, "i", &device))
	    return(NULL);
	
	status = cryptDeviceClose(device);
	
	return(processStatus(status));
}

static PyObject* python_cryptDeviceQueryCapability(PyObject* self, PyObject* args)
{
	int status = 0;
	CRYPT_QUERY_INFO cryptQueryInfo;
	int device = 0;
	int cryptAlgo = 0;
	
	if (!PyArg_ParseTuple(args, "ii", &device, &cryptAlgo))
	    return(NULL);
	
	status = cryptDeviceQueryCapability(device, cryptAlgo, &cryptQueryInfo);
	
	return(processStatusReturnCryptQueryInfo(status, cryptQueryInfo));
}

static PyObject* python_cryptDeviceCreateContext(PyObject* self, PyObject* args)
{
	int status = 0;
	int cryptContext = 0;
	int device = 0;
	int cryptAlgo = 0;
	
	if (!PyArg_ParseTuple(args, "ii", &device, &cryptAlgo))
	    return(NULL);
	
	status = cryptDeviceCreateContext(device, &cryptContext, cryptAlgo);
	
	return(processStatusReturnCryptHandle(status, cryptContext));
}

static PyObject* python_cryptLogin(PyObject* self, PyObject* args)
{
	int status = 0;
	int user = 0;
	PyObject* name = NULL;
	PyObject* password = NULL;
	unsigned char* namePtr = 0;
	unsigned char* passwordPtr = 0;
	
	if (!PyArg_ParseTuple(args, "OO", &name, &password))
	    return(NULL);
	
	if (!getPointerReadString(name, &namePtr))
		goto finish;
	if (!getPointerReadString(password, &passwordPtr))
		goto finish;
	
	status = cryptLogin(&user, namePtr, passwordPtr);
	
	finish:
	releasePointerString(name, namePtr);
	releasePointerString(password, passwordPtr);
	return(processStatusReturnCryptHandle(status, user));
}

static PyObject* python_cryptLogout(PyObject* self, PyObject* args)
{
	int status = 0;
	int user = 0;
	
	if (!PyArg_ParseTuple(args, "i", &user))
	    return(NULL);
	
	status = cryptLogout(user);
	
	return(processStatus(status));
}



static PyMethodDef module_functions[] =
{
	{ "cryptInit", python_cryptInit, METH_VARARGS }, 
	{ "cryptEnd", python_cryptEnd, METH_VARARGS }, 
	{ "cryptQueryCapability", python_cryptQueryCapability, METH_VARARGS }, 
	{ "cryptCreateContext", python_cryptCreateContext, METH_VARARGS }, 
	{ "cryptDestroyContext", python_cryptDestroyContext, METH_VARARGS }, 
	{ "cryptDestroyObject", python_cryptDestroyObject, METH_VARARGS }, 
	{ "cryptGenerateKey", python_cryptGenerateKey, METH_VARARGS }, 
	{ "cryptEncrypt", python_cryptEncrypt, METH_VARARGS }, 
	{ "cryptDecrypt", python_cryptDecrypt, METH_VARARGS }, 
	{ "cryptSetAttribute", python_cryptSetAttribute, METH_VARARGS }, 
	{ "cryptSetAttributeString", python_cryptSetAttributeString, METH_VARARGS }, 
	{ "cryptGetAttribute", python_cryptGetAttribute, METH_VARARGS }, 
	{ "cryptGetAttributeString", python_cryptGetAttributeString, METH_VARARGS }, 
	{ "cryptDeleteAttribute", python_cryptDeleteAttribute, METH_VARARGS }, 
	{ "cryptAddRandom", python_cryptAddRandom, METH_VARARGS }, 
	{ "cryptQueryObject", python_cryptQueryObject, METH_VARARGS }, 
	{ "cryptExportKey", python_cryptExportKey, METH_VARARGS }, 
	{ "cryptExportKeyEx", python_cryptExportKeyEx, METH_VARARGS }, 
	{ "cryptImportKey", python_cryptImportKey, METH_VARARGS }, 
	{ "cryptImportKeyEx", python_cryptImportKeyEx, METH_VARARGS }, 
	{ "cryptCreateSignature", python_cryptCreateSignature, METH_VARARGS }, 
	{ "cryptCreateSignatureEx", python_cryptCreateSignatureEx, METH_VARARGS }, 
	{ "cryptCheckSignature", python_cryptCheckSignature, METH_VARARGS }, 
	{ "cryptCheckSignatureEx", python_cryptCheckSignatureEx, METH_VARARGS }, 
	{ "cryptKeysetOpen", python_cryptKeysetOpen, METH_VARARGS }, 
	{ "cryptKeysetClose", python_cryptKeysetClose, METH_VARARGS }, 
	{ "cryptGetPublicKey", python_cryptGetPublicKey, METH_VARARGS }, 
	{ "cryptGetPrivateKey", python_cryptGetPrivateKey, METH_VARARGS }, 
	{ "cryptGetKey", python_cryptGetKey, METH_VARARGS }, 
	{ "cryptAddPublicKey", python_cryptAddPublicKey, METH_VARARGS }, 
	{ "cryptAddPrivateKey", python_cryptAddPrivateKey, METH_VARARGS }, 
	{ "cryptDeleteKey", python_cryptDeleteKey, METH_VARARGS }, 
	{ "cryptCreateCert", python_cryptCreateCert, METH_VARARGS }, 
	{ "cryptDestroyCert", python_cryptDestroyCert, METH_VARARGS }, 
	{ "cryptGetCertExtension", python_cryptGetCertExtension, METH_VARARGS }, 
	{ "cryptAddCertExtension", python_cryptAddCertExtension, METH_VARARGS }, 
	{ "cryptDeleteCertExtension", python_cryptDeleteCertExtension, METH_VARARGS }, 
	{ "cryptSignCert", python_cryptSignCert, METH_VARARGS }, 
	{ "cryptCheckCert", python_cryptCheckCert, METH_VARARGS }, 
	{ "cryptImportCert", python_cryptImportCert, METH_VARARGS }, 
	{ "cryptExportCert", python_cryptExportCert, METH_VARARGS }, 
	{ "cryptCAAddItem", python_cryptCAAddItem, METH_VARARGS }, 
	{ "cryptCAGetItem", python_cryptCAGetItem, METH_VARARGS }, 
	{ "cryptCADeleteItem", python_cryptCADeleteItem, METH_VARARGS }, 
	{ "cryptCACertManagement", python_cryptCACertManagement, METH_VARARGS }, 
	{ "cryptCreateEnvelope", python_cryptCreateEnvelope, METH_VARARGS }, 
	{ "cryptDestroyEnvelope", python_cryptDestroyEnvelope, METH_VARARGS }, 
	{ "cryptCreateSession", python_cryptCreateSession, METH_VARARGS }, 
	{ "cryptDestroySession", python_cryptDestroySession, METH_VARARGS }, 
	{ "cryptPushData", python_cryptPushData, METH_VARARGS }, 
	{ "cryptFlushData", python_cryptFlushData, METH_VARARGS }, 
	{ "cryptPopData", python_cryptPopData, METH_VARARGS }, 
	{ "cryptDeviceOpen", python_cryptDeviceOpen, METH_VARARGS }, 
	{ "cryptDeviceClose", python_cryptDeviceClose, METH_VARARGS }, 
	{ "cryptDeviceQueryCapability", python_cryptDeviceQueryCapability, METH_VARARGS }, 
	{ "cryptDeviceCreateContext", python_cryptDeviceCreateContext, METH_VARARGS }, 
	{ "cryptLogin", python_cryptLogin, METH_VARARGS }, 
	{ "cryptLogout", python_cryptLogout, METH_VARARGS }, 
    {0, 0}
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



    v = Py_BuildValue("i", CRYPT_ALGO_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_NONE", v);
    Py_DECREF(v); /* No encryption */

    v = Py_BuildValue("i", CRYPT_ALGO_DES);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_DES", v);
    Py_DECREF(v); /* DES */

    v = Py_BuildValue("i", CRYPT_ALGO_3DES);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_3DES", v);
    Py_DECREF(v); /* Triple DES */

    v = Py_BuildValue("i", CRYPT_ALGO_IDEA);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_IDEA", v);
    Py_DECREF(v); /* IDEA (only used for PGP 2.x) */

    v = Py_BuildValue("i", CRYPT_ALGO_CAST);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_CAST", v);
    Py_DECREF(v); /* CAST-128 (only used for OpenPGP) */

    v = Py_BuildValue("i", CRYPT_ALGO_RC2);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_RC2", v);
    Py_DECREF(v); /* RC2 (disabled by default, used for PKCS #12) */

    v = Py_BuildValue("i", CRYPT_ALGO_RC4);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_RC4", v);
    Py_DECREF(v); /* RC4 (insecure, deprecated) */

    v = Py_BuildValue("i", CRYPT_ALGO_RESERVED1);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_RESERVED1", v);
    Py_DECREF(v); /* Formerly RC5 */

    v = Py_BuildValue("i", CRYPT_ALGO_AES);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_AES", v);
    Py_DECREF(v); /* AES */

    v = Py_BuildValue("i", CRYPT_ALGO_RESERVED2);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_RESERVED2", v);
    Py_DECREF(v); /* Formerly Blowfish */

    v = Py_BuildValue("i", CRYPT_ALGO_DH);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_DH", v);
    Py_DECREF(v); /* Diffie-Hellman */

    v = Py_BuildValue("i", CRYPT_ALGO_RSA);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_RSA", v);
    Py_DECREF(v); /* RSA */

    v = Py_BuildValue("i", CRYPT_ALGO_DSA);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_DSA", v);
    Py_DECREF(v); /* DSA */

    v = Py_BuildValue("i", CRYPT_ALGO_ELGAMAL);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_ELGAMAL", v);
    Py_DECREF(v); /* ElGamal */

    v = Py_BuildValue("i", CRYPT_ALGO_RESERVED3);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_RESERVED3", v);
    Py_DECREF(v); /* Formerly KEA */

    v = Py_BuildValue("i", CRYPT_ALGO_ECDSA);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_ECDSA", v);
    Py_DECREF(v); /* ECDSA */

    v = Py_BuildValue("i", CRYPT_ALGO_ECDH);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_ECDH", v);
    Py_DECREF(v); /* ECDH */

    v = Py_BuildValue("i", CRYPT_ALGO_RESERVED4);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_RESERVED4", v);
    Py_DECREF(v); /* Formerly MD2 */

    v = Py_BuildValue("i", CRYPT_ALGO_RESERVED5);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_RESERVED5", v);
    Py_DECREF(v); /* Formerly MD4 */

    v = Py_BuildValue("i", CRYPT_ALGO_MD5);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_MD5", v);
    Py_DECREF(v); /* MD5 (only used for TLS 1.0/1.1) */

    v = Py_BuildValue("i", CRYPT_ALGO_SHA1);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_SHA1", v);
    Py_DECREF(v); /* SHA/SHA1 */

    v = Py_BuildValue("i", CRYPT_ALGO_RESERVED6);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_RESERVED6", v);
    Py_DECREF(v); /* Formerly RIPE-MD 160 */

    v = Py_BuildValue("i", CRYPT_ALGO_SHA2);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_SHA2", v);
    Py_DECREF(v); /* SHA-256 */

    v = Py_BuildValue("i", CRYPT_ALGO_SHA256);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_SHA256", v);
    Py_DECREF(v); /* Alternate name */

    v = Py_BuildValue("i", CRYPT_ALGO_SHAng);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_SHAng", v);
    Py_DECREF(v); /* Future SHA-nextgen standard */

    v = Py_BuildValue("i", CRYPT_ALGO_RESREVED_7);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_RESREVED_7", v);
    Py_DECREF(v); /* Formerly HMAC-MD5 */

    v = Py_BuildValue("i", CRYPT_ALGO_HMAC_SHA1);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_HMAC_SHA1", v);
    Py_DECREF(v); /* HMAC-SHA */

    v = Py_BuildValue("i", CRYPT_ALGO_RESERVED8);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_RESERVED8", v);
    Py_DECREF(v); /* Formerly HMAC-RIPEMD-160 */

    v = Py_BuildValue("i", CRYPT_ALGO_HMAC_SHA2);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_HMAC_SHA2", v);
    Py_DECREF(v); /* HMAC-SHA2 */

    v = Py_BuildValue("i", CRYPT_ALGO_HMAC_SHAng);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_HMAC_SHAng", v);
    Py_DECREF(v); /* HMAC-future-SHA-nextgen */

    v = Py_BuildValue("i", CRYPT_ALGO_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_LAST", v);
    Py_DECREF(v); /* Last possible crypt algo value */

    v = Py_BuildValue("i", CRYPT_ALGO_FIRST_CONVENTIONAL);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_FIRST_CONVENTIONAL", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_ALGO_LAST_CONVENTIONAL);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_LAST_CONVENTIONAL", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_ALGO_FIRST_PKC);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_FIRST_PKC", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_ALGO_LAST_PKC);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_LAST_PKC", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_ALGO_FIRST_HASH);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_FIRST_HASH", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_ALGO_LAST_HASH);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_LAST_HASH", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_ALGO_FIRST_MAC);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_FIRST_MAC", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_ALGO_LAST_MAC);
    PyDict_SetItemString(moduleDict, "CRYPT_ALGO_LAST_MAC", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_MODE_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_MODE_NONE", v);
    Py_DECREF(v); /* No encryption mode */

    v = Py_BuildValue("i", CRYPT_MODE_ECB);
    PyDict_SetItemString(moduleDict, "CRYPT_MODE_ECB", v);
    Py_DECREF(v); /* ECB */

    v = Py_BuildValue("i", CRYPT_MODE_CBC);
    PyDict_SetItemString(moduleDict, "CRYPT_MODE_CBC", v);
    Py_DECREF(v); /* CBC */

    v = Py_BuildValue("i", CRYPT_MODE_CFB);
    PyDict_SetItemString(moduleDict, "CRYPT_MODE_CFB", v);
    Py_DECREF(v); /* CFB */

    v = Py_BuildValue("i", CRYPT_MODE_GCM);
    PyDict_SetItemString(moduleDict, "CRYPT_MODE_GCM", v);
    Py_DECREF(v); /* GCM */

    v = Py_BuildValue("i", CRYPT_MODE_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_MODE_LAST", v);
    Py_DECREF(v); /* Last possible crypt mode value */

    v = Py_BuildValue("i", CRYPT_KEYSET_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYSET_NONE", v);
    Py_DECREF(v); /* No keyset type */

    v = Py_BuildValue("i", CRYPT_KEYSET_FILE);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYSET_FILE", v);
    Py_DECREF(v); /* Generic flat file keyset */

    v = Py_BuildValue("i", CRYPT_KEYSET_HTTP);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYSET_HTTP", v);
    Py_DECREF(v); /* Web page containing cert/CRL */

    v = Py_BuildValue("i", CRYPT_KEYSET_LDAP);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYSET_LDAP", v);
    Py_DECREF(v); /* LDAP directory service */

    v = Py_BuildValue("i", CRYPT_KEYSET_ODBC);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYSET_ODBC", v);
    Py_DECREF(v); /* Generic ODBC interface */

    v = Py_BuildValue("i", CRYPT_KEYSET_DATABASE);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYSET_DATABASE", v);
    Py_DECREF(v); /* Generic RDBMS interface */

    v = Py_BuildValue("i", CRYPT_KEYSET_ODBC_STORE);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYSET_ODBC_STORE", v);
    Py_DECREF(v); /* ODBC certificate store */

    v = Py_BuildValue("i", CRYPT_KEYSET_DATABASE_STORE);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYSET_DATABASE_STORE", v);
    Py_DECREF(v); /* Database certificate store */

    v = Py_BuildValue("i", CRYPT_KEYSET_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYSET_LAST", v);
    Py_DECREF(v); /* Last possible keyset type */

    v = Py_BuildValue("i", CRYPT_DEVICE_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_DEVICE_NONE", v);
    Py_DECREF(v); /* No crypto device */

    v = Py_BuildValue("i", CRYPT_DEVICE_FORTEZZA);
    PyDict_SetItemString(moduleDict, "CRYPT_DEVICE_FORTEZZA", v);
    Py_DECREF(v); /* Fortezza card - Placeholder only */

    v = Py_BuildValue("i", CRYPT_DEVICE_PKCS11);
    PyDict_SetItemString(moduleDict, "CRYPT_DEVICE_PKCS11", v);
    Py_DECREF(v); /* PKCS #11 crypto token */

    v = Py_BuildValue("i", CRYPT_DEVICE_CRYPTOAPI);
    PyDict_SetItemString(moduleDict, "CRYPT_DEVICE_CRYPTOAPI", v);
    Py_DECREF(v); /* Microsoft CryptoAPI */

    v = Py_BuildValue("i", CRYPT_DEVICE_HARDWARE);
    PyDict_SetItemString(moduleDict, "CRYPT_DEVICE_HARDWARE", v);
    Py_DECREF(v); /* Generic crypo HW plugin */

    v = Py_BuildValue("i", CRYPT_DEVICE_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_DEVICE_LAST", v);
    Py_DECREF(v); /* Last possible crypto device type */

    v = Py_BuildValue("i", CRYPT_CERTTYPE_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTTYPE_NONE", v);
    Py_DECREF(v); /* No certificate type */

    v = Py_BuildValue("i", CRYPT_CERTTYPE_CERTIFICATE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTTYPE_CERTIFICATE", v);
    Py_DECREF(v); /* Certificate */

    v = Py_BuildValue("i", CRYPT_CERTTYPE_ATTRIBUTE_CERT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTTYPE_ATTRIBUTE_CERT", v);
    Py_DECREF(v); /* Attribute certificate */

    v = Py_BuildValue("i", CRYPT_CERTTYPE_CERTCHAIN);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTTYPE_CERTCHAIN", v);
    Py_DECREF(v); /* PKCS #7 certificate chain */

    v = Py_BuildValue("i", CRYPT_CERTTYPE_CERTREQUEST);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTTYPE_CERTREQUEST", v);
    Py_DECREF(v); /* PKCS #10 certification request */

    v = Py_BuildValue("i", CRYPT_CERTTYPE_REQUEST_CERT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTTYPE_REQUEST_CERT", v);
    Py_DECREF(v); /* CRMF certification request */

    v = Py_BuildValue("i", CRYPT_CERTTYPE_REQUEST_REVOCATION);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTTYPE_REQUEST_REVOCATION", v);
    Py_DECREF(v); /* CRMF revocation request */

    v = Py_BuildValue("i", CRYPT_CERTTYPE_CRL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTTYPE_CRL", v);
    Py_DECREF(v); /* CRL */

    v = Py_BuildValue("i", CRYPT_CERTTYPE_CMS_ATTRIBUTES);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTTYPE_CMS_ATTRIBUTES", v);
    Py_DECREF(v); /* CMS attributes */

    v = Py_BuildValue("i", CRYPT_CERTTYPE_RTCS_REQUEST);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTTYPE_RTCS_REQUEST", v);
    Py_DECREF(v); /* RTCS request */

    v = Py_BuildValue("i", CRYPT_CERTTYPE_RTCS_RESPONSE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTTYPE_RTCS_RESPONSE", v);
    Py_DECREF(v); /* RTCS response */

    v = Py_BuildValue("i", CRYPT_CERTTYPE_OCSP_REQUEST);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTTYPE_OCSP_REQUEST", v);
    Py_DECREF(v); /* OCSP request */

    v = Py_BuildValue("i", CRYPT_CERTTYPE_OCSP_RESPONSE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTTYPE_OCSP_RESPONSE", v);
    Py_DECREF(v); /* OCSP response */

    v = Py_BuildValue("i", CRYPT_CERTTYPE_PKIUSER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTTYPE_PKIUSER", v);
    Py_DECREF(v); /* PKI user information */

    v = Py_BuildValue("i", CRYPT_CERTTYPE_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTTYPE_LAST", v);
    Py_DECREF(v); /* Last possible cert.type */

    v = Py_BuildValue("i", CRYPT_FORMAT_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_FORMAT_NONE", v);
    Py_DECREF(v); /* No format type */

    v = Py_BuildValue("i", CRYPT_FORMAT_AUTO);
    PyDict_SetItemString(moduleDict, "CRYPT_FORMAT_AUTO", v);
    Py_DECREF(v); /* Deenv, auto-determine type */

    v = Py_BuildValue("i", CRYPT_FORMAT_CRYPTLIB);
    PyDict_SetItemString(moduleDict, "CRYPT_FORMAT_CRYPTLIB", v);
    Py_DECREF(v); /* cryptlib native format */

    v = Py_BuildValue("i", CRYPT_FORMAT_CMS);
    PyDict_SetItemString(moduleDict, "CRYPT_FORMAT_CMS", v);
    Py_DECREF(v); /* PKCS #7 / CMS / S/MIME fmt. */

    v = Py_BuildValue("i", CRYPT_FORMAT_PKCS7);
    PyDict_SetItemString(moduleDict, "CRYPT_FORMAT_PKCS7", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_FORMAT_SMIME);
    PyDict_SetItemString(moduleDict, "CRYPT_FORMAT_SMIME", v);
    Py_DECREF(v); /* As CMS with MSG-style behaviour */

    v = Py_BuildValue("i", CRYPT_FORMAT_PGP);
    PyDict_SetItemString(moduleDict, "CRYPT_FORMAT_PGP", v);
    Py_DECREF(v); /* PGP format */

    v = Py_BuildValue("i", CRYPT_FORMAT_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_FORMAT_LAST", v);
    Py_DECREF(v); /* Last possible format type */

    v = Py_BuildValue("i", CRYPT_SESSION_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSION_NONE", v);
    Py_DECREF(v); /* No session type */

    v = Py_BuildValue("i", CRYPT_SESSION_SSH);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSION_SSH", v);
    Py_DECREF(v); /* SSH */

    v = Py_BuildValue("i", CRYPT_SESSION_SSH_SERVER);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSION_SSH_SERVER", v);
    Py_DECREF(v); /* SSH server */

    v = Py_BuildValue("i", CRYPT_SESSION_SSL);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSION_SSL", v);
    Py_DECREF(v); /* SSL/TLS */

    v = Py_BuildValue("i", CRYPT_SESSION_TLS);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSION_TLS", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_SESSION_SSL_SERVER);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSION_SSL_SERVER", v);
    Py_DECREF(v); /* SSL/TLS server */

    v = Py_BuildValue("i", CRYPT_SESSION_TLS_SERVER);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSION_TLS_SERVER", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_SESSION_RTCS);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSION_RTCS", v);
    Py_DECREF(v); /* RTCS */

    v = Py_BuildValue("i", CRYPT_SESSION_RTCS_SERVER);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSION_RTCS_SERVER", v);
    Py_DECREF(v); /* RTCS server */

    v = Py_BuildValue("i", CRYPT_SESSION_OCSP);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSION_OCSP", v);
    Py_DECREF(v); /* OCSP */

    v = Py_BuildValue("i", CRYPT_SESSION_OCSP_SERVER);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSION_OCSP_SERVER", v);
    Py_DECREF(v); /* OCSP server */

    v = Py_BuildValue("i", CRYPT_SESSION_TSP);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSION_TSP", v);
    Py_DECREF(v); /* TSP */

    v = Py_BuildValue("i", CRYPT_SESSION_TSP_SERVER);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSION_TSP_SERVER", v);
    Py_DECREF(v); /* TSP server */

    v = Py_BuildValue("i", CRYPT_SESSION_CMP);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSION_CMP", v);
    Py_DECREF(v); /* CMP */

    v = Py_BuildValue("i", CRYPT_SESSION_CMP_SERVER);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSION_CMP_SERVER", v);
    Py_DECREF(v); /* CMP server */

    v = Py_BuildValue("i", CRYPT_SESSION_SCEP);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSION_SCEP", v);
    Py_DECREF(v); /* SCEP */

    v = Py_BuildValue("i", CRYPT_SESSION_SCEP_SERVER);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSION_SCEP_SERVER", v);
    Py_DECREF(v); /* SCEP server */

    v = Py_BuildValue("i", CRYPT_SESSION_CERTSTORE_SERVER);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSION_CERTSTORE_SERVER", v);
    Py_DECREF(v); /* HTTP cert store interface */

    v = Py_BuildValue("i", CRYPT_SESSION_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSION_LAST", v);
    Py_DECREF(v); /* Last possible session type */

    v = Py_BuildValue("i", CRYPT_USER_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_USER_NONE", v);
    Py_DECREF(v); /* No user type */

    v = Py_BuildValue("i", CRYPT_USER_NORMAL);
    PyDict_SetItemString(moduleDict, "CRYPT_USER_NORMAL", v);
    Py_DECREF(v); /* Normal user */

    v = Py_BuildValue("i", CRYPT_USER_SO);
    PyDict_SetItemString(moduleDict, "CRYPT_USER_SO", v);
    Py_DECREF(v); /* Security officer */

    v = Py_BuildValue("i", CRYPT_USER_CA);
    PyDict_SetItemString(moduleDict, "CRYPT_USER_CA", v);
    Py_DECREF(v); /* CA user */

    v = Py_BuildValue("i", CRYPT_USER_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_USER_LAST", v);
    Py_DECREF(v); /* Last possible user type */

    v = Py_BuildValue("i", CRYPT_ATTRIBUTE_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_ATTRIBUTE_NONE", v);
    Py_DECREF(v); /* Non-value */

    v = Py_BuildValue("i", CRYPT_PROPERTY_FIRST);
    PyDict_SetItemString(moduleDict, "CRYPT_PROPERTY_FIRST", v);
    Py_DECREF(v); /* ******************* */

    v = Py_BuildValue("i", CRYPT_PROPERTY_HIGHSECURITY);
    PyDict_SetItemString(moduleDict, "CRYPT_PROPERTY_HIGHSECURITY", v);
    Py_DECREF(v); /* Owned+non-forwardcount+locked */

    v = Py_BuildValue("i", CRYPT_PROPERTY_OWNER);
    PyDict_SetItemString(moduleDict, "CRYPT_PROPERTY_OWNER", v);
    Py_DECREF(v); /* Object owner */

    v = Py_BuildValue("i", CRYPT_PROPERTY_FORWARDCOUNT);
    PyDict_SetItemString(moduleDict, "CRYPT_PROPERTY_FORWARDCOUNT", v);
    Py_DECREF(v); /* No.of times object can be forwarded */

    v = Py_BuildValue("i", CRYPT_PROPERTY_LOCKED);
    PyDict_SetItemString(moduleDict, "CRYPT_PROPERTY_LOCKED", v);
    Py_DECREF(v); /* Whether properties can be chged/read */

    v = Py_BuildValue("i", CRYPT_PROPERTY_USAGECOUNT);
    PyDict_SetItemString(moduleDict, "CRYPT_PROPERTY_USAGECOUNT", v);
    Py_DECREF(v); /* Usage count before object expires */

    v = Py_BuildValue("i", CRYPT_PROPERTY_NONEXPORTABLE);
    PyDict_SetItemString(moduleDict, "CRYPT_PROPERTY_NONEXPORTABLE", v);
    Py_DECREF(v); /* Whether key is nonexp.from context */

    v = Py_BuildValue("i", CRYPT_PROPERTY_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_PROPERTY_LAST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_GENERIC_FIRST);
    PyDict_SetItemString(moduleDict, "CRYPT_GENERIC_FIRST", v);
    Py_DECREF(v); /* Extended error information */

    v = Py_BuildValue("i", CRYPT_ATTRIBUTE_ERRORTYPE);
    PyDict_SetItemString(moduleDict, "CRYPT_ATTRIBUTE_ERRORTYPE", v);
    Py_DECREF(v); /* Type of last error */

    v = Py_BuildValue("i", CRYPT_ATTRIBUTE_ERRORLOCUS);
    PyDict_SetItemString(moduleDict, "CRYPT_ATTRIBUTE_ERRORLOCUS", v);
    Py_DECREF(v); /* Locus of last error */

    v = Py_BuildValue("i", CRYPT_ATTRIBUTE_ERRORMESSAGE);
    PyDict_SetItemString(moduleDict, "CRYPT_ATTRIBUTE_ERRORMESSAGE", v);
    Py_DECREF(v); /* Detailed error description */

    v = Py_BuildValue("i", CRYPT_ATTRIBUTE_CURRENT_GROUP);
    PyDict_SetItemString(moduleDict, "CRYPT_ATTRIBUTE_CURRENT_GROUP", v);
    Py_DECREF(v); /* Cursor mgt: Group in attribute list */

    v = Py_BuildValue("i", CRYPT_ATTRIBUTE_CURRENT);
    PyDict_SetItemString(moduleDict, "CRYPT_ATTRIBUTE_CURRENT", v);
    Py_DECREF(v); /* Cursor mgt: Entry in attribute list */

    v = Py_BuildValue("i", CRYPT_ATTRIBUTE_CURRENT_INSTANCE);
    PyDict_SetItemString(moduleDict, "CRYPT_ATTRIBUTE_CURRENT_INSTANCE", v);
    Py_DECREF(v); /* Cursor mgt: Instance in attribute list */

    v = Py_BuildValue("i", CRYPT_ATTRIBUTE_BUFFERSIZE);
    PyDict_SetItemString(moduleDict, "CRYPT_ATTRIBUTE_BUFFERSIZE", v);
    Py_DECREF(v); /* Internal data buffer size */

    v = Py_BuildValue("i", CRYPT_GENERIC_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_GENERIC_LAST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_OPTION_FIRST);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_FIRST", v);
    Py_DECREF(v); /* ************************** */

    v = Py_BuildValue("i", CRYPT_OPTION_INFO_DESCRIPTION);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_INFO_DESCRIPTION", v);
    Py_DECREF(v); /* Text description */

    v = Py_BuildValue("i", CRYPT_OPTION_INFO_COPYRIGHT);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_INFO_COPYRIGHT", v);
    Py_DECREF(v); /* Copyright notice */

    v = Py_BuildValue("i", CRYPT_OPTION_INFO_MAJORVERSION);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_INFO_MAJORVERSION", v);
    Py_DECREF(v); /* Major release version */

    v = Py_BuildValue("i", CRYPT_OPTION_INFO_MINORVERSION);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_INFO_MINORVERSION", v);
    Py_DECREF(v); /* Minor release version */

    v = Py_BuildValue("i", CRYPT_OPTION_INFO_STEPPING);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_INFO_STEPPING", v);
    Py_DECREF(v); /* Release stepping */

    v = Py_BuildValue("i", CRYPT_OPTION_ENCR_ALGO);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_ENCR_ALGO", v);
    Py_DECREF(v); /* Encryption algorithm */

    v = Py_BuildValue("i", CRYPT_OPTION_ENCR_HASH);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_ENCR_HASH", v);
    Py_DECREF(v); /* Hash algorithm */

    v = Py_BuildValue("i", CRYPT_OPTION_ENCR_MAC);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_ENCR_MAC", v);
    Py_DECREF(v); /* MAC algorithm */

    v = Py_BuildValue("i", CRYPT_OPTION_PKC_ALGO);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_PKC_ALGO", v);
    Py_DECREF(v); /* Public-key encryption algorithm */

    v = Py_BuildValue("i", CRYPT_OPTION_PKC_KEYSIZE);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_PKC_KEYSIZE", v);
    Py_DECREF(v); /* Public-key encryption key size */

    v = Py_BuildValue("i", CRYPT_OPTION_SIG_ALGO);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_SIG_ALGO", v);
    Py_DECREF(v); /* Signature algorithm */

    v = Py_BuildValue("i", CRYPT_OPTION_SIG_KEYSIZE);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_SIG_KEYSIZE", v);
    Py_DECREF(v); /* Signature keysize */

    v = Py_BuildValue("i", CRYPT_OPTION_KEYING_ALGO);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_KEYING_ALGO", v);
    Py_DECREF(v); /* Key processing algorithm */

    v = Py_BuildValue("i", CRYPT_OPTION_KEYING_ITERATIONS);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_KEYING_ITERATIONS", v);
    Py_DECREF(v); /* Key processing iterations */

    v = Py_BuildValue("i", CRYPT_OPTION_CERT_SIGNUNRECOGNISEDATTRIBUTES);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_CERT_SIGNUNRECOGNISEDATTRIBUTES", v);
    Py_DECREF(v); /* Whether to sign unrecog.attrs */

    v = Py_BuildValue("i", CRYPT_OPTION_CERT_VALIDITY);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_CERT_VALIDITY", v);
    Py_DECREF(v); /* Certificate validity period */

    v = Py_BuildValue("i", CRYPT_OPTION_CERT_UPDATEINTERVAL);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_CERT_UPDATEINTERVAL", v);
    Py_DECREF(v); /* CRL update interval */

    v = Py_BuildValue("i", CRYPT_OPTION_CERT_COMPLIANCELEVEL);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_CERT_COMPLIANCELEVEL", v);
    Py_DECREF(v); /* PKIX compliance level for cert chks. */

    v = Py_BuildValue("i", CRYPT_OPTION_CERT_REQUIREPOLICY);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_CERT_REQUIREPOLICY", v);
    Py_DECREF(v); /* Whether explicit policy req'd for certs */

    v = Py_BuildValue("i", CRYPT_OPTION_CMS_DEFAULTATTRIBUTES);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_CMS_DEFAULTATTRIBUTES", v);
    Py_DECREF(v); /* Add default CMS attributes */

    v = Py_BuildValue("i", CRYPT_OPTION_SMIME_DEFAULTATTRIBUTES);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_SMIME_DEFAULTATTRIBUTES", v);
    Py_DECREF(v); /* LDAP keyset options */

    v = Py_BuildValue("i", CRYPT_OPTION_KEYS_LDAP_OBJECTCLASS);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_KEYS_LDAP_OBJECTCLASS", v);
    Py_DECREF(v); /* Object class */

    v = Py_BuildValue("i", CRYPT_OPTION_KEYS_LDAP_OBJECTTYPE);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_KEYS_LDAP_OBJECTTYPE", v);
    Py_DECREF(v); /* Object type to fetch */

    v = Py_BuildValue("i", CRYPT_OPTION_KEYS_LDAP_FILTER);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_KEYS_LDAP_FILTER", v);
    Py_DECREF(v); /* Query filter */

    v = Py_BuildValue("i", CRYPT_OPTION_KEYS_LDAP_CACERTNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_KEYS_LDAP_CACERTNAME", v);
    Py_DECREF(v); /* CA certificate attribute name */

    v = Py_BuildValue("i", CRYPT_OPTION_KEYS_LDAP_CERTNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_KEYS_LDAP_CERTNAME", v);
    Py_DECREF(v); /* Certificate attribute name */

    v = Py_BuildValue("i", CRYPT_OPTION_KEYS_LDAP_CRLNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_KEYS_LDAP_CRLNAME", v);
    Py_DECREF(v); /* CRL attribute name */

    v = Py_BuildValue("i", CRYPT_OPTION_KEYS_LDAP_EMAILNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_KEYS_LDAP_EMAILNAME", v);
    Py_DECREF(v); /* Email attribute name */

    v = Py_BuildValue("i", CRYPT_OPTION_DEVICE_PKCS11_DVR01);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_DEVICE_PKCS11_DVR01", v);
    Py_DECREF(v); /* Name of first PKCS #11 driver */

    v = Py_BuildValue("i", CRYPT_OPTION_DEVICE_PKCS11_DVR02);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_DEVICE_PKCS11_DVR02", v);
    Py_DECREF(v); /* Name of second PKCS #11 driver */

    v = Py_BuildValue("i", CRYPT_OPTION_DEVICE_PKCS11_DVR03);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_DEVICE_PKCS11_DVR03", v);
    Py_DECREF(v); /* Name of third PKCS #11 driver */

    v = Py_BuildValue("i", CRYPT_OPTION_DEVICE_PKCS11_DVR04);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_DEVICE_PKCS11_DVR04", v);
    Py_DECREF(v); /* Name of fourth PKCS #11 driver */

    v = Py_BuildValue("i", CRYPT_OPTION_DEVICE_PKCS11_DVR05);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_DEVICE_PKCS11_DVR05", v);
    Py_DECREF(v); /* Name of fifth PKCS #11 driver */

    v = Py_BuildValue("i", CRYPT_OPTION_DEVICE_PKCS11_HARDWAREONLY);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_DEVICE_PKCS11_HARDWAREONLY", v);
    Py_DECREF(v); /* Use only hardware mechanisms */

    v = Py_BuildValue("i", CRYPT_OPTION_NET_SOCKS_SERVER);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_NET_SOCKS_SERVER", v);
    Py_DECREF(v); /* Socks server name */

    v = Py_BuildValue("i", CRYPT_OPTION_NET_SOCKS_USERNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_NET_SOCKS_USERNAME", v);
    Py_DECREF(v); /* Socks user name */

    v = Py_BuildValue("i", CRYPT_OPTION_NET_HTTP_PROXY);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_NET_HTTP_PROXY", v);
    Py_DECREF(v); /* Web proxy server */

    v = Py_BuildValue("i", CRYPT_OPTION_NET_CONNECTTIMEOUT);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_NET_CONNECTTIMEOUT", v);
    Py_DECREF(v); /* Timeout for network connection setup */

    v = Py_BuildValue("i", CRYPT_OPTION_NET_READTIMEOUT);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_NET_READTIMEOUT", v);
    Py_DECREF(v); /* Timeout for network reads */

    v = Py_BuildValue("i", CRYPT_OPTION_NET_WRITETIMEOUT);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_NET_WRITETIMEOUT", v);
    Py_DECREF(v); /* Timeout for network writes */

    v = Py_BuildValue("i", CRYPT_OPTION_MISC_ASYNCINIT);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_MISC_ASYNCINIT", v);
    Py_DECREF(v); /* Whether to init cryptlib async'ly */

    v = Py_BuildValue("i", CRYPT_OPTION_MISC_SIDECHANNELPROTECTION);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_MISC_SIDECHANNELPROTECTION", v);
    Py_DECREF(v); /* Protect against side-channel attacks */

    v = Py_BuildValue("i", CRYPT_OPTION_CONFIGCHANGED);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_CONFIGCHANGED", v);
    Py_DECREF(v); /* Whether in-mem.opts match on-disk ones */

    v = Py_BuildValue("i", CRYPT_OPTION_SELFTESTOK);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_SELFTESTOK", v);
    Py_DECREF(v); /* Whether self-test was completed and OK */

    v = Py_BuildValue("i", CRYPT_OPTION_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_OPTION_LAST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CTXINFO_FIRST);
    PyDict_SetItemString(moduleDict, "CRYPT_CTXINFO_FIRST", v);
    Py_DECREF(v); /* ******************** */

    v = Py_BuildValue("i", CRYPT_CTXINFO_ALGO);
    PyDict_SetItemString(moduleDict, "CRYPT_CTXINFO_ALGO", v);
    Py_DECREF(v); /* Algorithm */

    v = Py_BuildValue("i", CRYPT_CTXINFO_MODE);
    PyDict_SetItemString(moduleDict, "CRYPT_CTXINFO_MODE", v);
    Py_DECREF(v); /* Mode */

    v = Py_BuildValue("i", CRYPT_CTXINFO_NAME_ALGO);
    PyDict_SetItemString(moduleDict, "CRYPT_CTXINFO_NAME_ALGO", v);
    Py_DECREF(v); /* Algorithm name */

    v = Py_BuildValue("i", CRYPT_CTXINFO_NAME_MODE);
    PyDict_SetItemString(moduleDict, "CRYPT_CTXINFO_NAME_MODE", v);
    Py_DECREF(v); /* Mode name */

    v = Py_BuildValue("i", CRYPT_CTXINFO_KEYSIZE);
    PyDict_SetItemString(moduleDict, "CRYPT_CTXINFO_KEYSIZE", v);
    Py_DECREF(v); /* Key size in bytes */

    v = Py_BuildValue("i", CRYPT_CTXINFO_BLOCKSIZE);
    PyDict_SetItemString(moduleDict, "CRYPT_CTXINFO_BLOCKSIZE", v);
    Py_DECREF(v); /* Block size */

    v = Py_BuildValue("i", CRYPT_CTXINFO_IVSIZE);
    PyDict_SetItemString(moduleDict, "CRYPT_CTXINFO_IVSIZE", v);
    Py_DECREF(v); /* IV size */

    v = Py_BuildValue("i", CRYPT_CTXINFO_KEYING_ALGO);
    PyDict_SetItemString(moduleDict, "CRYPT_CTXINFO_KEYING_ALGO", v);
    Py_DECREF(v); /* Key processing algorithm */

    v = Py_BuildValue("i", CRYPT_CTXINFO_KEYING_ITERATIONS);
    PyDict_SetItemString(moduleDict, "CRYPT_CTXINFO_KEYING_ITERATIONS", v);
    Py_DECREF(v); /* Key processing iterations */

    v = Py_BuildValue("i", CRYPT_CTXINFO_KEYING_SALT);
    PyDict_SetItemString(moduleDict, "CRYPT_CTXINFO_KEYING_SALT", v);
    Py_DECREF(v); /* Key processing salt */

    v = Py_BuildValue("i", CRYPT_CTXINFO_KEYING_VALUE);
    PyDict_SetItemString(moduleDict, "CRYPT_CTXINFO_KEYING_VALUE", v);
    Py_DECREF(v); /* Value used to derive key */

    v = Py_BuildValue("i", CRYPT_CTXINFO_KEY);
    PyDict_SetItemString(moduleDict, "CRYPT_CTXINFO_KEY", v);
    Py_DECREF(v); /* Key */

    v = Py_BuildValue("i", CRYPT_CTXINFO_KEY_COMPONENTS);
    PyDict_SetItemString(moduleDict, "CRYPT_CTXINFO_KEY_COMPONENTS", v);
    Py_DECREF(v); /* Public-key components */

    v = Py_BuildValue("i", CRYPT_CTXINFO_IV);
    PyDict_SetItemString(moduleDict, "CRYPT_CTXINFO_IV", v);
    Py_DECREF(v); /* IV */

    v = Py_BuildValue("i", CRYPT_CTXINFO_HASHVALUE);
    PyDict_SetItemString(moduleDict, "CRYPT_CTXINFO_HASHVALUE", v);
    Py_DECREF(v); /* Hash value */

    v = Py_BuildValue("i", CRYPT_CTXINFO_LABEL);
    PyDict_SetItemString(moduleDict, "CRYPT_CTXINFO_LABEL", v);
    Py_DECREF(v); /* Label for private/secret key */

    v = Py_BuildValue("i", CRYPT_CTXINFO_PERSISTENT);
    PyDict_SetItemString(moduleDict, "CRYPT_CTXINFO_PERSISTENT", v);
    Py_DECREF(v); /* Obj.is backed by device or keyset */

    v = Py_BuildValue("i", CRYPT_CTXINFO_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_CTXINFO_LAST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_FIRST);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_FIRST", v);
    Py_DECREF(v); /* ************************ */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SELFSIGNED);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SELFSIGNED", v);
    Py_DECREF(v); /* Cert is self-signed */

    v = Py_BuildValue("i", CRYPT_CERTINFO_IMMUTABLE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_IMMUTABLE", v);
    Py_DECREF(v); /* Cert is signed and immutable */

    v = Py_BuildValue("i", CRYPT_CERTINFO_XYZZY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_XYZZY", v);
    Py_DECREF(v); /* Cert is a magic just-works cert */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CERTTYPE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CERTTYPE", v);
    Py_DECREF(v); /* Certificate object type */

    v = Py_BuildValue("i", CRYPT_CERTINFO_FINGERPRINT_SHA1);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_FINGERPRINT_SHA1", v);
    Py_DECREF(v); /* Certificate fingerprints */

    v = Py_BuildValue("i", CRYPT_CERTINFO_FINGERPRINT_SHA2);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_FINGERPRINT_SHA2", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_FINGERPRINT_SHAng);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_FINGERPRINT_SHAng", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_CURRENT_CERTIFICATE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CURRENT_CERTIFICATE", v);
    Py_DECREF(v); /* Cursor mgt: Rel.pos in chain/CRL/OCSP */

    v = Py_BuildValue("i", CRYPT_CERTINFO_TRUSTED_USAGE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_TRUSTED_USAGE", v);
    Py_DECREF(v); /* Usage that cert is trusted for */

    v = Py_BuildValue("i", CRYPT_CERTINFO_TRUSTED_IMPLICIT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_TRUSTED_IMPLICIT", v);
    Py_DECREF(v); /* Whether cert is implicitly trusted */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGNATURELEVEL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGNATURELEVEL", v);
    Py_DECREF(v); /* Amount of detail to include in sigs. */

    v = Py_BuildValue("i", CRYPT_CERTINFO_VERSION);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_VERSION", v);
    Py_DECREF(v); /* Cert.format version */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SERIALNUMBER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SERIALNUMBER", v);
    Py_DECREF(v); /* Serial number */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO", v);
    Py_DECREF(v); /* Public key */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CERTIFICATE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CERTIFICATE", v);
    Py_DECREF(v); /* User certificate */

    v = Py_BuildValue("i", CRYPT_CERTINFO_USERCERTIFICATE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_USERCERTIFICATE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_CACERTIFICATE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CACERTIFICATE", v);
    Py_DECREF(v); /* CA certificate */

    v = Py_BuildValue("i", CRYPT_CERTINFO_ISSUERNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_ISSUERNAME", v);
    Py_DECREF(v); /* Issuer DN */

    v = Py_BuildValue("i", CRYPT_CERTINFO_VALIDFROM);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_VALIDFROM", v);
    Py_DECREF(v); /* Cert valid-from time */

    v = Py_BuildValue("i", CRYPT_CERTINFO_VALIDTO);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_VALIDTO", v);
    Py_DECREF(v); /* Cert valid-to time */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SUBJECTNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SUBJECTNAME", v);
    Py_DECREF(v); /* Subject DN */

    v = Py_BuildValue("i", CRYPT_CERTINFO_ISSUERUNIQUEID);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_ISSUERUNIQUEID", v);
    Py_DECREF(v); /* Issuer unique ID */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SUBJECTUNIQUEID);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SUBJECTUNIQUEID", v);
    Py_DECREF(v); /* Subject unique ID */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CERTREQUEST);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CERTREQUEST", v);
    Py_DECREF(v); /* Cert.request (DN + public key) */

    v = Py_BuildValue("i", CRYPT_CERTINFO_THISUPDATE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_THISUPDATE", v);
    Py_DECREF(v); /* CRL/OCSP current-update time */

    v = Py_BuildValue("i", CRYPT_CERTINFO_NEXTUPDATE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_NEXTUPDATE", v);
    Py_DECREF(v); /* CRL/OCSP next-update time */

    v = Py_BuildValue("i", CRYPT_CERTINFO_REVOCATIONDATE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_REVOCATIONDATE", v);
    Py_DECREF(v); /* CRL/OCSP cert-revocation time */

    v = Py_BuildValue("i", CRYPT_CERTINFO_REVOCATIONSTATUS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_REVOCATIONSTATUS", v);
    Py_DECREF(v); /* OCSP revocation status */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CERTSTATUS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CERTSTATUS", v);
    Py_DECREF(v); /* RTCS certificate status */

    v = Py_BuildValue("i", CRYPT_CERTINFO_DN);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_DN", v);
    Py_DECREF(v); /* Currently selected DN in string form */

    v = Py_BuildValue("i", CRYPT_CERTINFO_PKIUSER_ID);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_PKIUSER_ID", v);
    Py_DECREF(v); /* PKI user ID */

    v = Py_BuildValue("i", CRYPT_CERTINFO_PKIUSER_ISSUEPASSWORD);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_PKIUSER_ISSUEPASSWORD", v);
    Py_DECREF(v); /* PKI user issue password */

    v = Py_BuildValue("i", CRYPT_CERTINFO_PKIUSER_REVPASSWORD);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_PKIUSER_REVPASSWORD", v);
    Py_DECREF(v); /* PKI user revocation password */

    v = Py_BuildValue("i", CRYPT_CERTINFO_PKIUSER_RA);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_PKIUSER_RA", v);
    Py_DECREF(v); /* PKI user is an RA */

    v = Py_BuildValue("i", CRYPT_CERTINFO_COUNTRYNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_COUNTRYNAME", v);
    Py_DECREF(v); /* countryName */

    v = Py_BuildValue("i", CRYPT_CERTINFO_STATEORPROVINCENAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_STATEORPROVINCENAME", v);
    Py_DECREF(v); /* stateOrProvinceName */

    v = Py_BuildValue("i", CRYPT_CERTINFO_LOCALITYNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_LOCALITYNAME", v);
    Py_DECREF(v); /* localityName */

    v = Py_BuildValue("i", CRYPT_CERTINFO_ORGANIZATIONNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_ORGANIZATIONNAME", v);
    Py_DECREF(v); /* organizationName */

    v = Py_BuildValue("i", CRYPT_CERTINFO_ORGANISATIONNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_ORGANISATIONNAME", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_ORGANIZATIONALUNITNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_ORGANIZATIONALUNITNAME", v);
    Py_DECREF(v); /* organizationalUnitName */

    v = Py_BuildValue("i", CRYPT_CERTINFO_ORGANISATIONALUNITNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_ORGANISATIONALUNITNAME", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_COMMONNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_COMMONNAME", v);
    Py_DECREF(v); /* commonName */

    v = Py_BuildValue("i", CRYPT_CERTINFO_OTHERNAME_TYPEID);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_OTHERNAME_TYPEID", v);
    Py_DECREF(v); /* otherName.typeID */

    v = Py_BuildValue("i", CRYPT_CERTINFO_OTHERNAME_VALUE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_OTHERNAME_VALUE", v);
    Py_DECREF(v); /* otherName.value */

    v = Py_BuildValue("i", CRYPT_CERTINFO_RFC822NAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_RFC822NAME", v);
    Py_DECREF(v); /* rfc822Name */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EMAIL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EMAIL", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_DNSNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_DNSNAME", v);
    Py_DECREF(v); /* dNSName */

    v = Py_BuildValue("i", CRYPT_CERTINFO_DIRECTORYNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_DIRECTORYNAME", v);
    Py_DECREF(v); /* directoryName */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EDIPARTYNAME_NAMEASSIGNER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EDIPARTYNAME_NAMEASSIGNER", v);
    Py_DECREF(v); /* ediPartyName.nameAssigner */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EDIPARTYNAME_PARTYNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EDIPARTYNAME_PARTYNAME", v);
    Py_DECREF(v); /* ediPartyName.partyName */

    v = Py_BuildValue("i", CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER", v);
    Py_DECREF(v); /* uniformResourceIdentifier */

    v = Py_BuildValue("i", CRYPT_CERTINFO_URL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_URL", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_IPADDRESS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_IPADDRESS", v);
    Py_DECREF(v); /* iPAddress */

    v = Py_BuildValue("i", CRYPT_CERTINFO_REGISTEREDID);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_REGISTEREDID", v);
    Py_DECREF(v); /* registeredID */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CHALLENGEPASSWORD);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CHALLENGEPASSWORD", v);
    Py_DECREF(v); /* 1 3 6 1 4 1 3029 3 1 4 cRLExtReason */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CRLEXTREASON);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CRLEXTREASON", v);
    Py_DECREF(v); /* 1 3 6 1 4 1 3029 3 1 5 keyFeatures */

    v = Py_BuildValue("i", CRYPT_CERTINFO_KEYFEATURES);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_KEYFEATURES", v);
    Py_DECREF(v); /* 1 3 6 1 5 5 7 1 1 authorityInfoAccess */

    v = Py_BuildValue("i", CRYPT_CERTINFO_AUTHORITYINFOACCESS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AUTHORITYINFOACCESS", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_AUTHORITYINFO_RTCS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AUTHORITYINFO_RTCS", v);
    Py_DECREF(v); /* accessDescription.accessLocation */

    v = Py_BuildValue("i", CRYPT_CERTINFO_AUTHORITYINFO_OCSP);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AUTHORITYINFO_OCSP", v);
    Py_DECREF(v); /* accessDescription.accessLocation */

    v = Py_BuildValue("i", CRYPT_CERTINFO_AUTHORITYINFO_CAISSUERS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AUTHORITYINFO_CAISSUERS", v);
    Py_DECREF(v); /* accessDescription.accessLocation */

    v = Py_BuildValue("i", CRYPT_CERTINFO_AUTHORITYINFO_CERTSTORE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AUTHORITYINFO_CERTSTORE", v);
    Py_DECREF(v); /* accessDescription.accessLocation */

    v = Py_BuildValue("i", CRYPT_CERTINFO_AUTHORITYINFO_CRLS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AUTHORITYINFO_CRLS", v);
    Py_DECREF(v); /* accessDescription.accessLocation */

    v = Py_BuildValue("i", CRYPT_CERTINFO_BIOMETRICINFO);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_BIOMETRICINFO", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_BIOMETRICINFO_TYPE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_BIOMETRICINFO_TYPE", v);
    Py_DECREF(v); /* biometricData.typeOfData */

    v = Py_BuildValue("i", CRYPT_CERTINFO_BIOMETRICINFO_HASHALGO);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_BIOMETRICINFO_HASHALGO", v);
    Py_DECREF(v); /* biometricData.hashAlgorithm */

    v = Py_BuildValue("i", CRYPT_CERTINFO_BIOMETRICINFO_HASH);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_BIOMETRICINFO_HASH", v);
    Py_DECREF(v); /* biometricData.dataHash */

    v = Py_BuildValue("i", CRYPT_CERTINFO_BIOMETRICINFO_URL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_BIOMETRICINFO_URL", v);
    Py_DECREF(v); /* biometricData.sourceDataUri */

    v = Py_BuildValue("i", CRYPT_CERTINFO_QCSTATEMENT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_QCSTATEMENT", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_QCSTATEMENT_SEMANTICS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_QCSTATEMENT_SEMANTICS", v);
    Py_DECREF(v); /* qcStatement.statementInfo.semanticsIdentifier */

    v = Py_BuildValue("i", CRYPT_CERTINFO_QCSTATEMENT_REGISTRATIONAUTHORITY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_QCSTATEMENT_REGISTRATIONAUTHORITY", v);
    Py_DECREF(v); /* qcStatement.statementInfo.nameRegistrationAuthorities */

    v = Py_BuildValue("i", CRYPT_CERTINFO_IPADDRESSBLOCKS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_IPADDRESSBLOCKS", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_IPADDRESSBLOCKS_ADDRESSFAMILY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_IPADDRESSBLOCKS_ADDRESSFAMILY", v);
    Py_DECREF(v); /* addressFamily */	CRYPT_CERTINFO_IPADDRESSBLOCKS_PREFIX,	/* ipAddress.addressPrefix */

    v = Py_BuildValue("i", CRYPT_CERTINFO_IPADDRESSBLOCKS_PREFIX);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_IPADDRESSBLOCKS_PREFIX", v);
    Py_DECREF(v); /* ipAddress.addressPrefix */

    v = Py_BuildValue("i", CRYPT_CERTINFO_IPADDRESSBLOCKS_MIN);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_IPADDRESSBLOCKS_MIN", v);
    Py_DECREF(v); /* ipAddress.addressRangeMin */

    v = Py_BuildValue("i", CRYPT_CERTINFO_IPADDRESSBLOCKS_MAX);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_IPADDRESSBLOCKS_MAX", v);
    Py_DECREF(v); /* ipAddress.addressRangeMax */

    v = Py_BuildValue("i", CRYPT_CERTINFO_AUTONOMOUSSYSIDS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AUTONOMOUSSYSIDS", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_ID);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_ID", v);
    Py_DECREF(v); /* asNum.id */

    v = Py_BuildValue("i", CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_MIN);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_MIN", v);
    Py_DECREF(v); /* asNum.min */

    v = Py_BuildValue("i", CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_MAX);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_MAX", v);
    Py_DECREF(v); /* asNum.max */

    v = Py_BuildValue("i", CRYPT_CERTINFO_OCSP_NONCE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_OCSP_NONCE", v);
    Py_DECREF(v); /* nonce */

    v = Py_BuildValue("i", CRYPT_CERTINFO_OCSP_RESPONSE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_OCSP_RESPONSE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_OCSP_RESPONSE_OCSP);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_OCSP_RESPONSE_OCSP", v);
    Py_DECREF(v); /* OCSP standard response */

    v = Py_BuildValue("i", CRYPT_CERTINFO_OCSP_NOCHECK);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_OCSP_NOCHECK", v);
    Py_DECREF(v); /* 1 3 6 1 5 5 7 48 1 6 ocspArchiveCutoff */

    v = Py_BuildValue("i", CRYPT_CERTINFO_OCSP_ARCHIVECUTOFF);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_OCSP_ARCHIVECUTOFF", v);
    Py_DECREF(v); /* 1 3 6 1 5 5 7 48 1 11 subjectInfoAccess */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SUBJECTINFOACCESS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SUBJECTINFOACCESS", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_SUBJECTINFO_TIMESTAMPING);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SUBJECTINFO_TIMESTAMPING", v);
    Py_DECREF(v); /* accessDescription.accessLocation */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SUBJECTINFO_CAREPOSITORY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SUBJECTINFO_CAREPOSITORY", v);
    Py_DECREF(v); /* accessDescription.accessLocation */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECTREPOSITORY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECTREPOSITORY", v);
    Py_DECREF(v); /* accessDescription.accessLocation */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SUBJECTINFO_RPKIMANIFEST);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SUBJECTINFO_RPKIMANIFEST", v);
    Py_DECREF(v); /* accessDescription.accessLocation */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECT", v);
    Py_DECREF(v); /* accessDescription.accessLocation */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_DATEOFCERTGEN);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_DATEOFCERTGEN", v);
    Py_DECREF(v); /* 1 3 36 8 3 2 siggProcuration */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_PROCURATION);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_PROCURATION", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_PROCURE_COUNTRY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_PROCURE_COUNTRY", v);
    Py_DECREF(v); /* country */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_PROCURE_TYPEOFSUBSTITUTION);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_PROCURE_TYPEOFSUBSTITUTION", v);
    Py_DECREF(v); /* typeOfSubstitution */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_PROCURE_SIGNINGFOR);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_PROCURE_SIGNINGFOR", v);
    Py_DECREF(v); /* signingFor.thirdPerson */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_ADMISSIONS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_ADMISSIONS", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_ADMISSIONS_AUTHORITY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_ADMISSIONS_AUTHORITY", v);
    Py_DECREF(v); /* authority */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHID);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHID", v);
    Py_DECREF(v); /* namingAuth.iD */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHURL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHURL", v);
    Py_DECREF(v); /* namingAuth.uRL */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHTEXT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHTEXT", v);
    Py_DECREF(v); /* namingAuth.text */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_ADMISSIONS_PROFESSIONITEM);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_ADMISSIONS_PROFESSIONITEM", v);
    Py_DECREF(v); /* professionItem */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_ADMISSIONS_PROFESSIONOID);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_ADMISSIONS_PROFESSIONOID", v);
    Py_DECREF(v); /* professionOID */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_ADMISSIONS_REGISTRATIONNUMBER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_ADMISSIONS_REGISTRATIONNUMBER", v);
    Py_DECREF(v); /* registrationNumber */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_MONETARYLIMIT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_MONETARYLIMIT", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_MONETARY_CURRENCY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_MONETARY_CURRENCY", v);
    Py_DECREF(v); /* currency */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_MONETARY_AMOUNT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_MONETARY_AMOUNT", v);
    Py_DECREF(v); /* amount */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_MONETARY_EXPONENT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_MONETARY_EXPONENT", v);
    Py_DECREF(v); /* exponent */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_DECLARATIONOFMAJORITY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_DECLARATIONOFMAJORITY", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_DECLARATIONOFMAJORITY_COUNTRY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_DECLARATIONOFMAJORITY_COUNTRY", v);
    Py_DECREF(v); /* fullAgeAtCountry */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_RESTRICTION);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_RESTRICTION", v);
    Py_DECREF(v); /* 1 3 36 8 3 13 siggCertHash */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_CERTHASH);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_CERTHASH", v);
    Py_DECREF(v); /* 1 3 36 8 3 15 siggAdditionalInformation */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SIGG_ADDITIONALINFORMATION);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SIGG_ADDITIONALINFORMATION", v);
    Py_DECREF(v); /* 1 3 101 1 4 1 strongExtranet */

    v = Py_BuildValue("i", CRYPT_CERTINFO_STRONGEXTRANET);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_STRONGEXTRANET", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_STRONGEXTRANET_ZONE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_STRONGEXTRANET_ZONE", v);
    Py_DECREF(v); /* sxNetIDList.sxNetID.zone */

    v = Py_BuildValue("i", CRYPT_CERTINFO_STRONGEXTRANET_ID);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_STRONGEXTRANET_ID", v);
    Py_DECREF(v); /* sxNetIDList.sxNetID.id */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SUBJECTDIRECTORYATTRIBUTES);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SUBJECTDIRECTORYATTRIBUTES", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_SUBJECTDIR_TYPE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SUBJECTDIR_TYPE", v);
    Py_DECREF(v); /* attribute.type */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SUBJECTDIR_VALUES);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SUBJECTDIR_VALUES", v);
    Py_DECREF(v); /* attribute.values */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER", v);
    Py_DECREF(v); /* 2 5 29 15 keyUsage */

    v = Py_BuildValue("i", CRYPT_CERTINFO_KEYUSAGE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_KEYUSAGE", v);
    Py_DECREF(v); /* 2 5 29 16 privateKeyUsagePeriod */

    v = Py_BuildValue("i", CRYPT_CERTINFO_PRIVATEKEYUSAGEPERIOD);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_PRIVATEKEYUSAGEPERIOD", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_PRIVATEKEY_NOTBEFORE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_PRIVATEKEY_NOTBEFORE", v);
    Py_DECREF(v); /* notBefore */

    v = Py_BuildValue("i", CRYPT_CERTINFO_PRIVATEKEY_NOTAFTER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_PRIVATEKEY_NOTAFTER", v);
    Py_DECREF(v); /* notAfter */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SUBJECTALTNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SUBJECTALTNAME", v);
    Py_DECREF(v); /* 2 5 29 18 issuerAltName */

    v = Py_BuildValue("i", CRYPT_CERTINFO_ISSUERALTNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_ISSUERALTNAME", v);
    Py_DECREF(v); /* 2 5 29 19 basicConstraints */

    v = Py_BuildValue("i", CRYPT_CERTINFO_BASICCONSTRAINTS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_BASICCONSTRAINTS", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_CA);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CA", v);
    Py_DECREF(v); /* cA */

    v = Py_BuildValue("i", CRYPT_CERTINFO_AUTHORITY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AUTHORITY", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_PATHLENCONSTRAINT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_PATHLENCONSTRAINT", v);
    Py_DECREF(v); /* pathLenConstraint */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CRLNUMBER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CRLNUMBER", v);
    Py_DECREF(v); /* 2 5 29 21 cRLReason */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CRLREASON);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CRLREASON", v);
    Py_DECREF(v); /* 2 5 29 23 holdInstructionCode */

    v = Py_BuildValue("i", CRYPT_CERTINFO_HOLDINSTRUCTIONCODE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_HOLDINSTRUCTIONCODE", v);
    Py_DECREF(v); /* 2 5 29 24 invalidityDate */

    v = Py_BuildValue("i", CRYPT_CERTINFO_INVALIDITYDATE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_INVALIDITYDATE", v);
    Py_DECREF(v); /* 2 5 29 27 deltaCRLIndicator */

    v = Py_BuildValue("i", CRYPT_CERTINFO_DELTACRLINDICATOR);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_DELTACRLINDICATOR", v);
    Py_DECREF(v); /* 2 5 29 28 issuingDistributionPoint */

    v = Py_BuildValue("i", CRYPT_CERTINFO_ISSUINGDISTRIBUTIONPOINT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_ISSUINGDISTRIBUTIONPOINT", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_ISSUINGDIST_FULLNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_ISSUINGDIST_FULLNAME", v);
    Py_DECREF(v); /* distributionPointName.fullName */

    v = Py_BuildValue("i", CRYPT_CERTINFO_ISSUINGDIST_USERCERTSONLY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_ISSUINGDIST_USERCERTSONLY", v);
    Py_DECREF(v); /* onlyContainsUserCerts */

    v = Py_BuildValue("i", CRYPT_CERTINFO_ISSUINGDIST_CACERTSONLY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_ISSUINGDIST_CACERTSONLY", v);
    Py_DECREF(v); /* onlyContainsCACerts */

    v = Py_BuildValue("i", CRYPT_CERTINFO_ISSUINGDIST_SOMEREASONSONLY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_ISSUINGDIST_SOMEREASONSONLY", v);
    Py_DECREF(v); /* onlySomeReasons */

    v = Py_BuildValue("i", CRYPT_CERTINFO_ISSUINGDIST_INDIRECTCRL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_ISSUINGDIST_INDIRECTCRL", v);
    Py_DECREF(v); /* indirectCRL */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CERTIFICATEISSUER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CERTIFICATEISSUER", v);
    Py_DECREF(v); /* 2 5 29 30 nameConstraints */

    v = Py_BuildValue("i", CRYPT_CERTINFO_NAMECONSTRAINTS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_NAMECONSTRAINTS", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_PERMITTEDSUBTREES);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_PERMITTEDSUBTREES", v);
    Py_DECREF(v); /* permittedSubtrees */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXCLUDEDSUBTREES);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXCLUDEDSUBTREES", v);
    Py_DECREF(v); /* excludedSubtrees */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CRLDISTRIBUTIONPOINT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CRLDISTRIBUTIONPOINT", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_CRLDIST_FULLNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CRLDIST_FULLNAME", v);
    Py_DECREF(v); /* distributionPointName.fullName */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CRLDIST_REASONS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CRLDIST_REASONS", v);
    Py_DECREF(v); /* reasons */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CRLDIST_CRLISSUER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CRLDIST_CRLISSUER", v);
    Py_DECREF(v); /* cRLIssuer */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CERTIFICATEPOLICIES);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CERTIFICATEPOLICIES", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_CERTPOLICYID);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CERTPOLICYID", v);
    Py_DECREF(v); /* policyInformation.policyIdentifier */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CERTPOLICY_CPSURI);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CERTPOLICY_CPSURI", v);
    Py_DECREF(v); /* policyInformation.policyQualifiers.qualifier.cPSuri */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CERTPOLICY_ORGANIZATION);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CERTPOLICY_ORGANIZATION", v);
    Py_DECREF(v); /* policyInformation.policyQualifiers.qualifier.userNotice.noticeRef.organization */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CERTPOLICY_NOTICENUMBERS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CERTPOLICY_NOTICENUMBERS", v);
    Py_DECREF(v); /* policyInformation.policyQualifiers.qualifier.userNotice.noticeRef.noticeNumbers */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CERTPOLICY_EXPLICITTEXT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CERTPOLICY_EXPLICITTEXT", v);
    Py_DECREF(v); /* policyInformation.policyQualifiers.qualifier.userNotice.explicitText */

    v = Py_BuildValue("i", CRYPT_CERTINFO_POLICYMAPPINGS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_POLICYMAPPINGS", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_ISSUERDOMAINPOLICY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_ISSUERDOMAINPOLICY", v);
    Py_DECREF(v); /* policyMappings.issuerDomainPolicy */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SUBJECTDOMAINPOLICY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SUBJECTDOMAINPOLICY", v);
    Py_DECREF(v); /* policyMappings.subjectDomainPolicy */

    v = Py_BuildValue("i", CRYPT_CERTINFO_AUTHORITYKEYIDENTIFIER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AUTHORITYKEYIDENTIFIER", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_AUTHORITY_KEYIDENTIFIER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AUTHORITY_KEYIDENTIFIER", v);
    Py_DECREF(v); /* keyIdentifier */

    v = Py_BuildValue("i", CRYPT_CERTINFO_AUTHORITY_CERTISSUER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AUTHORITY_CERTISSUER", v);
    Py_DECREF(v); /* authorityCertIssuer */

    v = Py_BuildValue("i", CRYPT_CERTINFO_AUTHORITY_CERTSERIALNUMBER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AUTHORITY_CERTSERIALNUMBER", v);
    Py_DECREF(v); /* authorityCertSerialNumber */

    v = Py_BuildValue("i", CRYPT_CERTINFO_POLICYCONSTRAINTS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_POLICYCONSTRAINTS", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_REQUIREEXPLICITPOLICY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_REQUIREEXPLICITPOLICY", v);
    Py_DECREF(v); /* policyConstraints.requireExplicitPolicy */

    v = Py_BuildValue("i", CRYPT_CERTINFO_INHIBITPOLICYMAPPING);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_INHIBITPOLICYMAPPING", v);
    Py_DECREF(v); /* policyConstraints.inhibitPolicyMapping */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEYUSAGE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEYUSAGE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEY_MS_INDIVIDUALCODESIGNING);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEY_MS_INDIVIDUALCODESIGNING", v);
    Py_DECREF(v); /* individualCodeSigning */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEY_MS_COMMERCIALCODESIGNING);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEY_MS_COMMERCIALCODESIGNING", v);
    Py_DECREF(v); /* commercialCodeSigning */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEY_MS_CERTTRUSTLISTSIGNING);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEY_MS_CERTTRUSTLISTSIGNING", v);
    Py_DECREF(v); /* certTrustListSigning */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEY_MS_TIMESTAMPSIGNING);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEY_MS_TIMESTAMPSIGNING", v);
    Py_DECREF(v); /* timeStampSigning */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEY_MS_SERVERGATEDCRYPTO);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEY_MS_SERVERGATEDCRYPTO", v);
    Py_DECREF(v); /* serverGatedCrypto */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEY_MS_ENCRYPTEDFILESYSTEM);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEY_MS_ENCRYPTEDFILESYSTEM", v);
    Py_DECREF(v); /* encrypedFileSystem */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEY_SERVERAUTH);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEY_SERVERAUTH", v);
    Py_DECREF(v); /* serverAuth */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEY_CLIENTAUTH);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEY_CLIENTAUTH", v);
    Py_DECREF(v); /* clientAuth */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEY_CODESIGNING);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEY_CODESIGNING", v);
    Py_DECREF(v); /* codeSigning */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEY_EMAILPROTECTION);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEY_EMAILPROTECTION", v);
    Py_DECREF(v); /* emailProtection */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEY_IPSECENDSYSTEM);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEY_IPSECENDSYSTEM", v);
    Py_DECREF(v); /* ipsecEndSystem */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEY_IPSECTUNNEL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEY_IPSECTUNNEL", v);
    Py_DECREF(v); /* ipsecTunnel */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEY_IPSECUSER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEY_IPSECUSER", v);
    Py_DECREF(v); /* ipsecUser */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEY_TIMESTAMPING);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEY_TIMESTAMPING", v);
    Py_DECREF(v); /* timeStamping */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEY_OCSPSIGNING);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEY_OCSPSIGNING", v);
    Py_DECREF(v); /* ocspSigning */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEY_DIRECTORYSERVICE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEY_DIRECTORYSERVICE", v);
    Py_DECREF(v); /* directoryService */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEY_ANYKEYUSAGE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEY_ANYKEYUSAGE", v);
    Py_DECREF(v); /* anyExtendedKeyUsage */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEY_NS_SERVERGATEDCRYPTO);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEY_NS_SERVERGATEDCRYPTO", v);
    Py_DECREF(v); /* serverGatedCrypto */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXTKEY_VS_SERVERGATEDCRYPTO_CA);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXTKEY_VS_SERVERGATEDCRYPTO_CA", v);
    Py_DECREF(v); /* serverGatedCrypto CA */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CRLSTREAMIDENTIFIER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CRLSTREAMIDENTIFIER", v);
    Py_DECREF(v); /* 2 5 29 46 freshestCRL */

    v = Py_BuildValue("i", CRYPT_CERTINFO_FRESHESTCRL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_FRESHESTCRL", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_FRESHESTCRL_FULLNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_FRESHESTCRL_FULLNAME", v);
    Py_DECREF(v); /* distributionPointName.fullName */

    v = Py_BuildValue("i", CRYPT_CERTINFO_FRESHESTCRL_REASONS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_FRESHESTCRL_REASONS", v);
    Py_DECREF(v); /* reasons */

    v = Py_BuildValue("i", CRYPT_CERTINFO_FRESHESTCRL_CRLISSUER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_FRESHESTCRL_CRLISSUER", v);
    Py_DECREF(v); /* cRLIssuer */

    v = Py_BuildValue("i", CRYPT_CERTINFO_ORDEREDLIST);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_ORDEREDLIST", v);
    Py_DECREF(v); /* 2 5 29 51 baseUpdateTime */

    v = Py_BuildValue("i", CRYPT_CERTINFO_BASEUPDATETIME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_BASEUPDATETIME", v);
    Py_DECREF(v); /* 2 5 29 53 deltaInfo */

    v = Py_BuildValue("i", CRYPT_CERTINFO_DELTAINFO);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_DELTAINFO", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_DELTAINFO_LOCATION);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_DELTAINFO_LOCATION", v);
    Py_DECREF(v); /* deltaLocation */

    v = Py_BuildValue("i", CRYPT_CERTINFO_DELTAINFO_NEXTDELTA);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_DELTAINFO_NEXTDELTA", v);
    Py_DECREF(v); /* nextDelta */

    v = Py_BuildValue("i", CRYPT_CERTINFO_INHIBITANYPOLICY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_INHIBITANYPOLICY", v);
    Py_DECREF(v); /* 2 5 29 58 toBeRevoked */

    v = Py_BuildValue("i", CRYPT_CERTINFO_TOBEREVOKED);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_TOBEREVOKED", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_TOBEREVOKED_CERTISSUER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_TOBEREVOKED_CERTISSUER", v);
    Py_DECREF(v); /* certificateIssuer */

    v = Py_BuildValue("i", CRYPT_CERTINFO_TOBEREVOKED_REASONCODE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_TOBEREVOKED_REASONCODE", v);
    Py_DECREF(v); /* reasonCode */

    v = Py_BuildValue("i", CRYPT_CERTINFO_TOBEREVOKED_REVOCATIONTIME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_TOBEREVOKED_REVOCATIONTIME", v);
    Py_DECREF(v); /* revocationTime */

    v = Py_BuildValue("i", CRYPT_CERTINFO_TOBEREVOKED_CERTSERIALNUMBER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_TOBEREVOKED_CERTSERIALNUMBER", v);
    Py_DECREF(v); /* certSerialNumber */

    v = Py_BuildValue("i", CRYPT_CERTINFO_REVOKEDGROUPS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_REVOKEDGROUPS", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_REVOKEDGROUPS_CERTISSUER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_REVOKEDGROUPS_CERTISSUER", v);
    Py_DECREF(v); /* certificateIssuer */

    v = Py_BuildValue("i", CRYPT_CERTINFO_REVOKEDGROUPS_REASONCODE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_REVOKEDGROUPS_REASONCODE", v);
    Py_DECREF(v); /* reasonCode */

    v = Py_BuildValue("i", CRYPT_CERTINFO_REVOKEDGROUPS_INVALIDITYDATE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_REVOKEDGROUPS_INVALIDITYDATE", v);
    Py_DECREF(v); /* invalidityDate */

    v = Py_BuildValue("i", CRYPT_CERTINFO_REVOKEDGROUPS_STARTINGNUMBER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_REVOKEDGROUPS_STARTINGNUMBER", v);
    Py_DECREF(v); /* startingNumber */

    v = Py_BuildValue("i", CRYPT_CERTINFO_REVOKEDGROUPS_ENDINGNUMBER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_REVOKEDGROUPS_ENDINGNUMBER", v);
    Py_DECREF(v); /* endingNumber */

    v = Py_BuildValue("i", CRYPT_CERTINFO_EXPIREDCERTSONCRL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_EXPIREDCERTSONCRL", v);
    Py_DECREF(v); /* 2 5 29 63 aaIssuingDistributionPoint */

    v = Py_BuildValue("i", CRYPT_CERTINFO_AAISSUINGDISTRIBUTIONPOINT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AAISSUINGDISTRIBUTIONPOINT", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_AAISSUINGDIST_FULLNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AAISSUINGDIST_FULLNAME", v);
    Py_DECREF(v); /* distributionPointName.fullName */

    v = Py_BuildValue("i", CRYPT_CERTINFO_AAISSUINGDIST_SOMEREASONSONLY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AAISSUINGDIST_SOMEREASONSONLY", v);
    Py_DECREF(v); /* onlySomeReasons */

    v = Py_BuildValue("i", CRYPT_CERTINFO_AAISSUINGDIST_INDIRECTCRL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AAISSUINGDIST_INDIRECTCRL", v);
    Py_DECREF(v); /* indirectCRL */

    v = Py_BuildValue("i", CRYPT_CERTINFO_AAISSUINGDIST_USERATTRCERTS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AAISSUINGDIST_USERATTRCERTS", v);
    Py_DECREF(v); /* containsUserAttributeCerts */

    v = Py_BuildValue("i", CRYPT_CERTINFO_AAISSUINGDIST_AACERTS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AAISSUINGDIST_AACERTS", v);
    Py_DECREF(v); /* containsAACerts */

    v = Py_BuildValue("i", CRYPT_CERTINFO_AAISSUINGDIST_SOACERTS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_AAISSUINGDIST_SOACERTS", v);
    Py_DECREF(v); /* containsSOAPublicKeyCerts */

    v = Py_BuildValue("i", CRYPT_CERTINFO_NS_CERTTYPE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_NS_CERTTYPE", v);
    Py_DECREF(v); /* netscape-cert-type */

    v = Py_BuildValue("i", CRYPT_CERTINFO_NS_BASEURL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_NS_BASEURL", v);
    Py_DECREF(v); /* netscape-base-url */

    v = Py_BuildValue("i", CRYPT_CERTINFO_NS_REVOCATIONURL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_NS_REVOCATIONURL", v);
    Py_DECREF(v); /* netscape-revocation-url */

    v = Py_BuildValue("i", CRYPT_CERTINFO_NS_CAREVOCATIONURL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_NS_CAREVOCATIONURL", v);
    Py_DECREF(v); /* netscape-ca-revocation-url */

    v = Py_BuildValue("i", CRYPT_CERTINFO_NS_CERTRENEWALURL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_NS_CERTRENEWALURL", v);
    Py_DECREF(v); /* netscape-cert-renewal-url */

    v = Py_BuildValue("i", CRYPT_CERTINFO_NS_CAPOLICYURL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_NS_CAPOLICYURL", v);
    Py_DECREF(v); /* netscape-ca-policy-url */

    v = Py_BuildValue("i", CRYPT_CERTINFO_NS_SSLSERVERNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_NS_SSLSERVERNAME", v);
    Py_DECREF(v); /* netscape-ssl-server-name */

    v = Py_BuildValue("i", CRYPT_CERTINFO_NS_COMMENT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_NS_COMMENT", v);
    Py_DECREF(v); /* netscape-comment */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_HASHEDROOTKEY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_HASHEDROOTKEY", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_ROOTKEYTHUMBPRINT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_ROOTKEYTHUMBPRINT", v);
    Py_DECREF(v); /* rootKeyThumbPrint */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_CERTIFICATETYPE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_CERTIFICATETYPE", v);
    Py_DECREF(v); /* 2 23 42 7 2 SET merchantData */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_MERCHANTDATA);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_MERCHANTDATA", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_MERID);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_MERID", v);
    Py_DECREF(v); /* merID */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_MERACQUIRERBIN);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_MERACQUIRERBIN", v);
    Py_DECREF(v); /* merAcquirerBIN */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_MERCHANTLANGUAGE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_MERCHANTLANGUAGE", v);
    Py_DECREF(v); /* merNames.language */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_MERCHANTNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_MERCHANTNAME", v);
    Py_DECREF(v); /* merNames.name */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_MERCHANTCITY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_MERCHANTCITY", v);
    Py_DECREF(v); /* merNames.city */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_MERCHANTSTATEPROVINCE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_MERCHANTSTATEPROVINCE", v);
    Py_DECREF(v); /* merNames.stateProvince */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_MERCHANTPOSTALCODE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_MERCHANTPOSTALCODE", v);
    Py_DECREF(v); /* merNames.postalCode */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_MERCHANTCOUNTRYNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_MERCHANTCOUNTRYNAME", v);
    Py_DECREF(v); /* merNames.countryName */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_MERCOUNTRY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_MERCOUNTRY", v);
    Py_DECREF(v); /* merCountry */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_MERAUTHFLAG);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_MERAUTHFLAG", v);
    Py_DECREF(v); /* merAuthFlag */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_CERTCARDREQUIRED);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_CERTCARDREQUIRED", v);
    Py_DECREF(v); /* 2 23 42 7 4 SET tunneling */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_TUNNELING);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_TUNNELING", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_TUNNELLING);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_TUNNELLING", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_TUNNELINGFLAG);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_TUNNELINGFLAG", v);
    Py_DECREF(v); /* tunneling */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_TUNNELLINGFLAG);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_TUNNELLINGFLAG", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_TUNNELINGALGID);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_TUNNELINGALGID", v);
    Py_DECREF(v); /* tunnelingAlgID */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SET_TUNNELLINGALGID);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SET_TUNNELLINGALGID", v);
    Py_DECREF(v); /* S/MIME attributes */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_CONTENTTYPE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_CONTENTTYPE", v);
    Py_DECREF(v); /* 1 2 840 113549 1 9 4 messageDigest */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_MESSAGEDIGEST);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_MESSAGEDIGEST", v);
    Py_DECREF(v); /* 1 2 840 113549 1 9 5 signingTime */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGNINGTIME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGNINGTIME", v);
    Py_DECREF(v); /* 1 2 840 113549 1 9 6 counterSignature */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_COUNTERSIGNATURE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_COUNTERSIGNATURE", v);
    Py_DECREF(v); /* counterSignature */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGNINGDESCRIPTION);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGNINGDESCRIPTION", v);
    Py_DECREF(v); /* 1 2 840 113549 1 9 15 sMIMECapabilities */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAPABILITIES);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAPABILITIES", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_3DES);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_3DES", v);
    Py_DECREF(v); /* 3DES encryption */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_AES);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_AES", v);
    Py_DECREF(v); /* AES encryption */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_CAST128);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_CAST128", v);
    Py_DECREF(v); /* CAST-128 encryption */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_SHAng);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_SHAng", v);
    Py_DECREF(v); /* SHA2-ng hash */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_SHA2);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_SHA2", v);
    Py_DECREF(v); /* SHA2-256 hash */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_SHA1);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_SHA1", v);
    Py_DECREF(v); /* SHA1 hash */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHAng);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHAng", v);
    Py_DECREF(v); /* HMAC-SHA2-ng MAC */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA2);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA2", v);
    Py_DECREF(v); /* HMAC-SHA2-256 MAC */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA1);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA1", v);
    Py_DECREF(v); /* HMAC-SHA1 MAC */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC256);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC256", v);
    Py_DECREF(v); /* AuthEnc w.256-bit key */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC128);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC128", v);
    Py_DECREF(v); /* AuthEnc w.128-bit key */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHAng);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHAng", v);
    Py_DECREF(v); /* RSA with SHA-ng signing */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA2);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA2", v);
    Py_DECREF(v); /* RSA with SHA2-256 signing */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA1);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA1", v);
    Py_DECREF(v); /* RSA with SHA1 signing */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_DSA_SHA1);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_DSA_SHA1", v);
    Py_DECREF(v); /* DSA with SHA-1 signing */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHAng);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHAng", v);
    Py_DECREF(v); /* ECDSA with SHA-ng signing */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA2);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA2", v);
    Py_DECREF(v); /* ECDSA with SHA2-256 signing */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA1);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA1", v);
    Py_DECREF(v); /* ECDSA with SHA-1 signing */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_PREFERSIGNEDDATA);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_PREFERSIGNEDDATA", v);
    Py_DECREF(v); /* preferSignedData */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_CANNOTDECRYPTANY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_CANNOTDECRYPTANY", v);
    Py_DECREF(v); /* canNotDecryptAny */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SMIMECAP_PREFERBINARYINSIDE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SMIMECAP_PREFERBINARYINSIDE", v);
    Py_DECREF(v); /* preferBinaryInside */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_RECEIPTREQUEST);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_RECEIPTREQUEST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_RECEIPT_CONTENTIDENTIFIER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_RECEIPT_CONTENTIDENTIFIER", v);
    Py_DECREF(v); /* contentIdentifier */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_RECEIPT_FROM);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_RECEIPT_FROM", v);
    Py_DECREF(v); /* receiptsFrom */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_RECEIPT_TO);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_RECEIPT_TO", v);
    Py_DECREF(v); /* receiptsTo */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SECURITYLABEL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SECURITYLABEL", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SECLABEL_POLICY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SECLABEL_POLICY", v);
    Py_DECREF(v); /* securityPolicyIdentifier */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SECLABEL_CLASSIFICATION);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SECLABEL_CLASSIFICATION", v);
    Py_DECREF(v); /* securityClassification */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SECLABEL_PRIVACYMARK);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SECLABEL_PRIVACYMARK", v);
    Py_DECREF(v); /* privacyMark */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SECLABEL_CATTYPE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SECLABEL_CATTYPE", v);
    Py_DECREF(v); /* securityCategories.securityCategory.type */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SECLABEL_CATVALUE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SECLABEL_CATVALUE", v);
    Py_DECREF(v); /* securityCategories.securityCategory.value */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_MLEXPANSIONHISTORY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_MLEXPANSIONHISTORY", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_MLEXP_ENTITYIDENTIFIER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_MLEXP_ENTITYIDENTIFIER", v);
    Py_DECREF(v); /* mlData.mailListIdentifier.issuerAndSerialNumber */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_MLEXP_TIME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_MLEXP_TIME", v);
    Py_DECREF(v); /* mlData.expansionTime */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_MLEXP_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_MLEXP_NONE", v);
    Py_DECREF(v); /* mlData.mlReceiptPolicy.none */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_MLEXP_INSTEADOF);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_MLEXP_INSTEADOF", v);
    Py_DECREF(v); /* mlData.mlReceiptPolicy.insteadOf.generalNames.generalName */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_MLEXP_INADDITIONTO);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_MLEXP_INADDITIONTO", v);
    Py_DECREF(v); /* mlData.mlReceiptPolicy.inAdditionTo.generalNames.generalName */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_CONTENTHINTS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_CONTENTHINTS", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_CONTENTHINT_DESCRIPTION);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_CONTENTHINT_DESCRIPTION", v);
    Py_DECREF(v); /* contentDescription */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_CONTENTHINT_TYPE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_CONTENTHINT_TYPE", v);
    Py_DECREF(v); /* contentType */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_EQUIVALENTLABEL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_EQUIVALENTLABEL", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_EQVLABEL_POLICY);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_EQVLABEL_POLICY", v);
    Py_DECREF(v); /* securityPolicyIdentifier */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_EQVLABEL_CLASSIFICATION);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_EQVLABEL_CLASSIFICATION", v);
    Py_DECREF(v); /* securityClassification */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_EQVLABEL_PRIVACYMARK);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_EQVLABEL_PRIVACYMARK", v);
    Py_DECREF(v); /* privacyMark */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_EQVLABEL_CATTYPE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_EQVLABEL_CATTYPE", v);
    Py_DECREF(v); /* securityCategories.securityCategory.type */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_EQVLABEL_CATVALUE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_EQVLABEL_CATVALUE", v);
    Py_DECREF(v); /* securityCategories.securityCategory.value */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGNINGCERT_ESSCERTID);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGNINGCERT_ESSCERTID", v);
    Py_DECREF(v); /* certs.essCertID */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGNINGCERT_POLICIES);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGNINGCERT_POLICIES", v);
    Py_DECREF(v); /* policies.policyInformation.policyIdentifier */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATEV2);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATEV2", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGNINGCERTV2_ESSCERTIDV2);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGNINGCERTV2_ESSCERTIDV2", v);
    Py_DECREF(v); /* certs.essCertID */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGNINGCERTV2_POLICIES);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGNINGCERTV2_POLICIES", v);
    Py_DECREF(v); /* policies.policyInformation.policyIdentifier */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGNATUREPOLICYID);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGNATUREPOLICYID", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGPOLICYID);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGPOLICYID", v);
    Py_DECREF(v); /* sigPolicyID */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGPOLICYHASH);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGPOLICYHASH", v);
    Py_DECREF(v); /* sigPolicyHash */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGPOLICY_CPSURI);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGPOLICY_CPSURI", v);
    Py_DECREF(v); /* sigPolicyQualifiers.sigPolicyQualifier.cPSuri */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGPOLICY_ORGANIZATION);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGPOLICY_ORGANIZATION", v);
    Py_DECREF(v); /* sigPolicyQualifiers.sigPolicyQualifier.userNotice.noticeRef.organization */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGPOLICY_NOTICENUMBERS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGPOLICY_NOTICENUMBERS", v);
    Py_DECREF(v); /* sigPolicyQualifiers.sigPolicyQualifier.userNotice.noticeRef.noticeNumbers */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGPOLICY_EXPLICITTEXT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGPOLICY_EXPLICITTEXT", v);
    Py_DECREF(v); /* sigPolicyQualifiers.sigPolicyQualifier.userNotice.explicitText */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGTYPEIDENTIFIER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGTYPEIDENTIFIER", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGTYPEID_ORIGINATORSIG);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGTYPEID_ORIGINATORSIG", v);
    Py_DECREF(v); /* originatorSig */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGTYPEID_DOMAINSIG);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGTYPEID_DOMAINSIG", v);
    Py_DECREF(v); /* domainSig */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGTYPEID_ADDITIONALATTRIBUTES);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGTYPEID_ADDITIONALATTRIBUTES", v);
    Py_DECREF(v); /* additionalAttributesSig */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SIGTYPEID_REVIEWSIG);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SIGTYPEID_REVIEWSIG", v);
    Py_DECREF(v); /* reviewSig */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_NONCE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_NONCE", v);
    Py_DECREF(v); /* randomNonce */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SCEP_MESSAGETYPE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SCEP_MESSAGETYPE", v);
    Py_DECREF(v); /* messageType */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SCEP_PKISTATUS);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SCEP_PKISTATUS", v);
    Py_DECREF(v); /* pkiStatus */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SCEP_FAILINFO);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SCEP_FAILINFO", v);
    Py_DECREF(v); /* failInfo */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SCEP_SENDERNONCE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SCEP_SENDERNONCE", v);
    Py_DECREF(v); /* senderNonce */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SCEP_RECIPIENTNONCE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SCEP_RECIPIENTNONCE", v);
    Py_DECREF(v); /* recipientNonce */

    v = Py_BuildValue("i", CRYPT_CERTINFO_SCEP_TRANSACTIONID);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_SCEP_TRANSACTIONID", v);
    Py_DECREF(v); /* transID */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SPCAGENCYINFO);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SPCAGENCYINFO", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SPCAGENCYURL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SPCAGENCYURL", v);
    Py_DECREF(v); /* spcAgencyInfo.url */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SPCSTATEMENTTYPE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SPCSTATEMENTTYPE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SPCSTMT_INDIVIDUALCODESIGNING);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SPCSTMT_INDIVIDUALCODESIGNING", v);
    Py_DECREF(v); /* individualCodeSigning */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SPCSTMT_COMMERCIALCODESIGNING);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SPCSTMT_COMMERCIALCODESIGNING", v);
    Py_DECREF(v); /* commercialCodeSigning */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SPCOPUSINFO);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SPCOPUSINFO", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SPCOPUSINFO_NAME);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SPCOPUSINFO_NAME", v);
    Py_DECREF(v); /* spcOpusInfo.name */

    v = Py_BuildValue("i", CRYPT_CERTINFO_CMS_SPCOPUSINFO_URL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_CMS_SPCOPUSINFO_URL", v);
    Py_DECREF(v); /* spcOpusInfo.url */

    v = Py_BuildValue("i", CRYPT_CERTINFO_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTINFO_LAST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_KEYINFO_FIRST);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYINFO_FIRST", v);
    Py_DECREF(v); /* ******************* */

    v = Py_BuildValue("i", CRYPT_KEYINFO_QUERY);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYINFO_QUERY", v);
    Py_DECREF(v); /* Keyset query */

    v = Py_BuildValue("i", CRYPT_KEYINFO_QUERY_REQUESTS);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYINFO_QUERY_REQUESTS", v);
    Py_DECREF(v); /* Query of requests in cert store */

    v = Py_BuildValue("i", CRYPT_KEYINFO_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYINFO_LAST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_DEVINFO_FIRST);
    PyDict_SetItemString(moduleDict, "CRYPT_DEVINFO_FIRST", v);
    Py_DECREF(v); /* ******************* */

    v = Py_BuildValue("i", CRYPT_DEVINFO_INITIALISE);
    PyDict_SetItemString(moduleDict, "CRYPT_DEVINFO_INITIALISE", v);
    Py_DECREF(v); /* Initialise device for use */

    v = Py_BuildValue("i", CRYPT_DEVINFO_INITIALIZE);
    PyDict_SetItemString(moduleDict, "CRYPT_DEVINFO_INITIALIZE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_DEVINFO_AUTHENT_USER);
    PyDict_SetItemString(moduleDict, "CRYPT_DEVINFO_AUTHENT_USER", v);
    Py_DECREF(v); /* Authenticate user to device */

    v = Py_BuildValue("i", CRYPT_DEVINFO_AUTHENT_SUPERVISOR);
    PyDict_SetItemString(moduleDict, "CRYPT_DEVINFO_AUTHENT_SUPERVISOR", v);
    Py_DECREF(v); /* Authenticate supervisor to dev. */

    v = Py_BuildValue("i", CRYPT_DEVINFO_SET_AUTHENT_USER);
    PyDict_SetItemString(moduleDict, "CRYPT_DEVINFO_SET_AUTHENT_USER", v);
    Py_DECREF(v); /* Set user authent.value */

    v = Py_BuildValue("i", CRYPT_DEVINFO_SET_AUTHENT_SUPERVISOR);
    PyDict_SetItemString(moduleDict, "CRYPT_DEVINFO_SET_AUTHENT_SUPERVISOR", v);
    Py_DECREF(v); /* Set supervisor auth.val. */

    v = Py_BuildValue("i", CRYPT_DEVINFO_ZEROISE);
    PyDict_SetItemString(moduleDict, "CRYPT_DEVINFO_ZEROISE", v);
    Py_DECREF(v); /* Zeroise device */

    v = Py_BuildValue("i", CRYPT_DEVINFO_ZEROIZE);
    PyDict_SetItemString(moduleDict, "CRYPT_DEVINFO_ZEROIZE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_DEVINFO_LOGGEDIN);
    PyDict_SetItemString(moduleDict, "CRYPT_DEVINFO_LOGGEDIN", v);
    Py_DECREF(v); /* Whether user is logged in */

    v = Py_BuildValue("i", CRYPT_DEVINFO_LABEL);
    PyDict_SetItemString(moduleDict, "CRYPT_DEVINFO_LABEL", v);
    Py_DECREF(v); /* Device/token label */

    v = Py_BuildValue("i", CRYPT_DEVINFO_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_DEVINFO_LAST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_ENVINFO_FIRST);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_FIRST", v);
    Py_DECREF(v); /* ********************* */

    v = Py_BuildValue("i", CRYPT_ENVINFO_DATASIZE);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_DATASIZE", v);
    Py_DECREF(v); /* Data size information */

    v = Py_BuildValue("i", CRYPT_ENVINFO_COMPRESSION);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_COMPRESSION", v);
    Py_DECREF(v); /* Compression information */

    v = Py_BuildValue("i", CRYPT_ENVINFO_CONTENTTYPE);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_CONTENTTYPE", v);
    Py_DECREF(v); /* Inner CMS content type */

    v = Py_BuildValue("i", CRYPT_ENVINFO_DETACHEDSIGNATURE);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_DETACHEDSIGNATURE", v);
    Py_DECREF(v); /* Detached signature */

    v = Py_BuildValue("i", CRYPT_ENVINFO_SIGNATURE_RESULT);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_SIGNATURE_RESULT", v);
    Py_DECREF(v); /* Signature check result */

    v = Py_BuildValue("i", CRYPT_ENVINFO_INTEGRITY);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_INTEGRITY", v);
    Py_DECREF(v); /* Integrity-protection level */

    v = Py_BuildValue("i", CRYPT_ENVINFO_PASSWORD);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_PASSWORD", v);
    Py_DECREF(v); /* User password */

    v = Py_BuildValue("i", CRYPT_ENVINFO_KEY);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_KEY", v);
    Py_DECREF(v); /* Conventional encryption key */

    v = Py_BuildValue("i", CRYPT_ENVINFO_SIGNATURE);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_SIGNATURE", v);
    Py_DECREF(v); /* Signature/signature check key */

    v = Py_BuildValue("i", CRYPT_ENVINFO_SIGNATURE_EXTRADATA);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_SIGNATURE_EXTRADATA", v);
    Py_DECREF(v); /* Extra information added to CMS sigs */

    v = Py_BuildValue("i", CRYPT_ENVINFO_RECIPIENT);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_RECIPIENT", v);
    Py_DECREF(v); /* Recipient email address */

    v = Py_BuildValue("i", CRYPT_ENVINFO_PUBLICKEY);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_PUBLICKEY", v);
    Py_DECREF(v); /* PKC encryption key */

    v = Py_BuildValue("i", CRYPT_ENVINFO_PRIVATEKEY);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_PRIVATEKEY", v);
    Py_DECREF(v); /* PKC decryption key */

    v = Py_BuildValue("i", CRYPT_ENVINFO_PRIVATEKEY_LABEL);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_PRIVATEKEY_LABEL", v);
    Py_DECREF(v); /* Label of PKC decryption key */

    v = Py_BuildValue("i", CRYPT_ENVINFO_ORIGINATOR);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_ORIGINATOR", v);
    Py_DECREF(v); /* Originator info/key */

    v = Py_BuildValue("i", CRYPT_ENVINFO_SESSIONKEY);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_SESSIONKEY", v);
    Py_DECREF(v); /* Session key */

    v = Py_BuildValue("i", CRYPT_ENVINFO_HASH);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_HASH", v);
    Py_DECREF(v); /* Hash value */

    v = Py_BuildValue("i", CRYPT_ENVINFO_TIMESTAMP);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_TIMESTAMP", v);
    Py_DECREF(v); /* Timestamp information */

    v = Py_BuildValue("i", CRYPT_ENVINFO_KEYSET_SIGCHECK);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_KEYSET_SIGCHECK", v);
    Py_DECREF(v); /* Signature check keyset */

    v = Py_BuildValue("i", CRYPT_ENVINFO_KEYSET_ENCRYPT);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_KEYSET_ENCRYPT", v);
    Py_DECREF(v); /* PKC encryption keyset */

    v = Py_BuildValue("i", CRYPT_ENVINFO_KEYSET_DECRYPT);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_KEYSET_DECRYPT", v);
    Py_DECREF(v); /* PKC decryption keyset */

    v = Py_BuildValue("i", CRYPT_ENVINFO_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVINFO_LAST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_SESSINFO_FIRST);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_FIRST", v);
    Py_DECREF(v); /* ******************** */

    v = Py_BuildValue("i", CRYPT_SESSINFO_ACTIVE);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_ACTIVE", v);
    Py_DECREF(v); /* Whether session is active */

    v = Py_BuildValue("i", CRYPT_SESSINFO_CONNECTIONACTIVE);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_CONNECTIONACTIVE", v);
    Py_DECREF(v); /* Whether network connection is active */

    v = Py_BuildValue("i", CRYPT_SESSINFO_USERNAME);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_USERNAME", v);
    Py_DECREF(v); /* User name */

    v = Py_BuildValue("i", CRYPT_SESSINFO_PASSWORD);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_PASSWORD", v);
    Py_DECREF(v); /* Password */

    v = Py_BuildValue("i", CRYPT_SESSINFO_PRIVATEKEY);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_PRIVATEKEY", v);
    Py_DECREF(v); /* Server/client private key */

    v = Py_BuildValue("i", CRYPT_SESSINFO_KEYSET);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_KEYSET", v);
    Py_DECREF(v); /* Certificate store */

    v = Py_BuildValue("i", CRYPT_SESSINFO_AUTHRESPONSE);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_AUTHRESPONSE", v);
    Py_DECREF(v); /* Session authorisation OK */

    v = Py_BuildValue("i", CRYPT_SESSINFO_SERVER_NAME);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_SERVER_NAME", v);
    Py_DECREF(v); /* Server name */

    v = Py_BuildValue("i", CRYPT_SESSINFO_SERVER_PORT);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_SERVER_PORT", v);
    Py_DECREF(v); /* Server port number */

    v = Py_BuildValue("i", CRYPT_SESSINFO_SERVER_FINGERPRINT_SHA1);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_SERVER_FINGERPRINT_SHA1", v);
    Py_DECREF(v); /* Server key fingerprint */

    v = Py_BuildValue("i", CRYPT_SESSINFO_CLIENT_NAME);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_CLIENT_NAME", v);
    Py_DECREF(v); /* Client name */

    v = Py_BuildValue("i", CRYPT_SESSINFO_CLIENT_PORT);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_CLIENT_PORT", v);
    Py_DECREF(v); /* Client port number */

    v = Py_BuildValue("i", CRYPT_SESSINFO_SESSION);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_SESSION", v);
    Py_DECREF(v); /* Transport mechanism */

    v = Py_BuildValue("i", CRYPT_SESSINFO_NETWORKSOCKET);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_NETWORKSOCKET", v);
    Py_DECREF(v); /* User-supplied network socket */

    v = Py_BuildValue("i", CRYPT_SESSINFO_VERSION);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_VERSION", v);
    Py_DECREF(v); /* Protocol version */

    v = Py_BuildValue("i", CRYPT_SESSINFO_REQUEST);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_REQUEST", v);
    Py_DECREF(v); /* Cert.request object */

    v = Py_BuildValue("i", CRYPT_SESSINFO_RESPONSE);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_RESPONSE", v);
    Py_DECREF(v); /* Cert.response object */

    v = Py_BuildValue("i", CRYPT_SESSINFO_CACERTIFICATE);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_CACERTIFICATE", v);
    Py_DECREF(v); /* Issuing CA certificate */

    v = Py_BuildValue("i", CRYPT_SESSINFO_CMP_REQUESTTYPE);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_CMP_REQUESTTYPE", v);
    Py_DECREF(v); /* Request type */

    v = Py_BuildValue("i", CRYPT_SESSINFO_CMP_PRIVKEYSET);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_CMP_PRIVKEYSET", v);
    Py_DECREF(v); /* Private-key keyset */

    v = Py_BuildValue("i", CRYPT_SESSINFO_SSH_CHANNEL);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_SSH_CHANNEL", v);
    Py_DECREF(v); /* SSH current channel */

    v = Py_BuildValue("i", CRYPT_SESSINFO_SSH_CHANNEL_TYPE);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_SSH_CHANNEL_TYPE", v);
    Py_DECREF(v); /* SSH channel type */

    v = Py_BuildValue("i", CRYPT_SESSINFO_SSH_CHANNEL_ARG1);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_SSH_CHANNEL_ARG1", v);
    Py_DECREF(v); /* SSH channel argument 1 */

    v = Py_BuildValue("i", CRYPT_SESSINFO_SSH_CHANNEL_ARG2);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_SSH_CHANNEL_ARG2", v);
    Py_DECREF(v); /* SSH channel argument 2 */

    v = Py_BuildValue("i", CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE", v);
    Py_DECREF(v); /* SSH channel active */

    v = Py_BuildValue("i", CRYPT_SESSINFO_SSL_OPTIONS);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_SSL_OPTIONS", v);
    Py_DECREF(v); /* SSL/TLS protocol options */

    v = Py_BuildValue("i", CRYPT_SESSINFO_TSP_MSGIMPRINT);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_TSP_MSGIMPRINT", v);
    Py_DECREF(v); /* TSP message imprint */

    v = Py_BuildValue("i", CRYPT_SESSINFO_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_SESSINFO_LAST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_USERINFO_FIRST);
    PyDict_SetItemString(moduleDict, "CRYPT_USERINFO_FIRST", v);
    Py_DECREF(v); /* ******************** */

    v = Py_BuildValue("i", CRYPT_USERINFO_PASSWORD);
    PyDict_SetItemString(moduleDict, "CRYPT_USERINFO_PASSWORD", v);
    Py_DECREF(v); /* Password */

    v = Py_BuildValue("i", CRYPT_USERINFO_CAKEY_CERTSIGN);
    PyDict_SetItemString(moduleDict, "CRYPT_USERINFO_CAKEY_CERTSIGN", v);
    Py_DECREF(v); /* CA cert signing key */

    v = Py_BuildValue("i", CRYPT_USERINFO_CAKEY_CRLSIGN);
    PyDict_SetItemString(moduleDict, "CRYPT_USERINFO_CAKEY_CRLSIGN", v);
    Py_DECREF(v); /* CA CRL signing key */

    v = Py_BuildValue("i", CRYPT_USERINFO_CAKEY_RTCSSIGN);
    PyDict_SetItemString(moduleDict, "CRYPT_USERINFO_CAKEY_RTCSSIGN", v);
    Py_DECREF(v); /* CA RTCS signing key */

    v = Py_BuildValue("i", CRYPT_USERINFO_CAKEY_OCSPSIGN);
    PyDict_SetItemString(moduleDict, "CRYPT_USERINFO_CAKEY_OCSPSIGN", v);
    Py_DECREF(v); /* CA OCSP signing key */

    v = Py_BuildValue("i", CRYPT_USERINFO_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_USERINFO_LAST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_ATTRIBUTE_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_ATTRIBUTE_LAST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CONTENT_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_CONTENT_NONE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CONTENT_DATA);
    PyDict_SetItemString(moduleDict, "CRYPT_CONTENT_DATA", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CONTENT_SIGNEDDATA);
    PyDict_SetItemString(moduleDict, "CRYPT_CONTENT_SIGNEDDATA", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CONTENT_ENVELOPEDDATA);
    PyDict_SetItemString(moduleDict, "CRYPT_CONTENT_ENVELOPEDDATA", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CONTENT_SIGNEDANDENVELOPEDDATA);
    PyDict_SetItemString(moduleDict, "CRYPT_CONTENT_SIGNEDANDENVELOPEDDATA", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CONTENT_DIGESTEDDATA);
    PyDict_SetItemString(moduleDict, "CRYPT_CONTENT_DIGESTEDDATA", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CONTENT_ENCRYPTEDDATA);
    PyDict_SetItemString(moduleDict, "CRYPT_CONTENT_ENCRYPTEDDATA", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CONTENT_COMPRESSEDDATA);
    PyDict_SetItemString(moduleDict, "CRYPT_CONTENT_COMPRESSEDDATA", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CONTENT_AUTHDATA);
    PyDict_SetItemString(moduleDict, "CRYPT_CONTENT_AUTHDATA", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CONTENT_AUTHENVDATA);
    PyDict_SetItemString(moduleDict, "CRYPT_CONTENT_AUTHENVDATA", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CONTENT_TSTINFO);
    PyDict_SetItemString(moduleDict, "CRYPT_CONTENT_TSTINFO", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CONTENT_SPCINDIRECTDATACONTEXT);
    PyDict_SetItemString(moduleDict, "CRYPT_CONTENT_SPCINDIRECTDATACONTEXT", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CONTENT_RTCSREQUEST);
    PyDict_SetItemString(moduleDict, "CRYPT_CONTENT_RTCSREQUEST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CONTENT_RTCSRESPONSE);
    PyDict_SetItemString(moduleDict, "CRYPT_CONTENT_RTCSRESPONSE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CONTENT_RTCSRESPONSE_EXT);
    PyDict_SetItemString(moduleDict, "CRYPT_CONTENT_RTCSRESPONSE_EXT", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CONTENT_MRTD);
    PyDict_SetItemString(moduleDict, "CRYPT_CONTENT_MRTD", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CONTENT_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_CONTENT_LAST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_SIGNATURELEVEL_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_SIGNATURELEVEL_NONE", v);
    Py_DECREF(v); /* Include only signature */

    v = Py_BuildValue("i", CRYPT_SIGNATURELEVEL_SIGNERCERT);
    PyDict_SetItemString(moduleDict, "CRYPT_SIGNATURELEVEL_SIGNERCERT", v);
    Py_DECREF(v); /* Include signer cert */

    v = Py_BuildValue("i", CRYPT_SIGNATURELEVEL_ALL);
    PyDict_SetItemString(moduleDict, "CRYPT_SIGNATURELEVEL_ALL", v);
    Py_DECREF(v); /* Include all relevant info */

    v = Py_BuildValue("i", CRYPT_SIGNATURELEVEL_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_SIGNATURELEVEL_LAST", v);
    Py_DECREF(v); /* Last possible sig.level type */

    v = Py_BuildValue("i", CRYPT_INTEGRITY_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_INTEGRITY_NONE", v);
    Py_DECREF(v); /* No integrity protection */

    v = Py_BuildValue("i", CRYPT_INTEGRITY_MACONLY);
    PyDict_SetItemString(moduleDict, "CRYPT_INTEGRITY_MACONLY", v);
    Py_DECREF(v); /* MAC only, no encryption */

    v = Py_BuildValue("i", CRYPT_INTEGRITY_FULL);
    PyDict_SetItemString(moduleDict, "CRYPT_INTEGRITY_FULL", v);
    Py_DECREF(v); /* Encryption + ingerity protection */

    v = Py_BuildValue("i", CRYPT_CERTFORMAT_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTFORMAT_NONE", v);
    Py_DECREF(v); /* No certificate format */

    v = Py_BuildValue("i", CRYPT_CERTFORMAT_CERTIFICATE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTFORMAT_CERTIFICATE", v);
    Py_DECREF(v); /* DER-encoded certificate */

    v = Py_BuildValue("i", CRYPT_CERTFORMAT_CERTCHAIN);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTFORMAT_CERTCHAIN", v);
    Py_DECREF(v); /* PKCS #7 certificate chain */

    v = Py_BuildValue("i", CRYPT_CERTFORMAT_TEXT_CERTIFICATE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTFORMAT_TEXT_CERTIFICATE", v);
    Py_DECREF(v); /* base-64 wrapped cert */

    v = Py_BuildValue("i", CRYPT_CERTFORMAT_TEXT_CERTCHAIN);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTFORMAT_TEXT_CERTCHAIN", v);
    Py_DECREF(v); /* base-64 wrapped cert chain */

    v = Py_BuildValue("i", CRYPT_CERTFORMAT_XML_CERTIFICATE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTFORMAT_XML_CERTIFICATE", v);
    Py_DECREF(v); /* XML wrapped cert */

    v = Py_BuildValue("i", CRYPT_CERTFORMAT_XML_CERTCHAIN);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTFORMAT_XML_CERTCHAIN", v);
    Py_DECREF(v); /* XML wrapped cert chain */

    v = Py_BuildValue("i", CRYPT_CERTFORMAT_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTFORMAT_LAST", v);
    Py_DECREF(v); /* Last possible cert.format type */

    v = Py_BuildValue("i", CRYPT_REQUESTTYPE_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_REQUESTTYPE_NONE", v);
    Py_DECREF(v); /* No request type */

    v = Py_BuildValue("i", CRYPT_REQUESTTYPE_INITIALISATION);
    PyDict_SetItemString(moduleDict, "CRYPT_REQUESTTYPE_INITIALISATION", v);
    Py_DECREF(v); /* Initialisation request */

    v = Py_BuildValue("i", CRYPT_REQUESTTYPE_INITIALIZATION);
    PyDict_SetItemString(moduleDict, "CRYPT_REQUESTTYPE_INITIALIZATION", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_REQUESTTYPE_CERTIFICATE);
    PyDict_SetItemString(moduleDict, "CRYPT_REQUESTTYPE_CERTIFICATE", v);
    Py_DECREF(v); /* Certification request */

    v = Py_BuildValue("i", CRYPT_REQUESTTYPE_KEYUPDATE);
    PyDict_SetItemString(moduleDict, "CRYPT_REQUESTTYPE_KEYUPDATE", v);
    Py_DECREF(v); /* Key update request */

    v = Py_BuildValue("i", CRYPT_REQUESTTYPE_REVOCATION);
    PyDict_SetItemString(moduleDict, "CRYPT_REQUESTTYPE_REVOCATION", v);
    Py_DECREF(v); /* Cert revocation request */

    v = Py_BuildValue("i", CRYPT_REQUESTTYPE_PKIBOOT);
    PyDict_SetItemString(moduleDict, "CRYPT_REQUESTTYPE_PKIBOOT", v);
    Py_DECREF(v); /* PKIBoot request */

    v = Py_BuildValue("i", CRYPT_REQUESTTYPE_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_REQUESTTYPE_LAST", v);
    Py_DECREF(v); /* Last possible request type */

    v = Py_BuildValue("i", CRYPT_KEYID_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYID_NONE", v);
    Py_DECREF(v); /* No key ID type */

    v = Py_BuildValue("i", CRYPT_KEYID_NAME);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYID_NAME", v);
    Py_DECREF(v); /* Key owner name */

    v = Py_BuildValue("i", CRYPT_KEYID_URI);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYID_URI", v);
    Py_DECREF(v); /* Key owner URI */

    v = Py_BuildValue("i", CRYPT_KEYID_EMAIL);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYID_EMAIL", v);
    Py_DECREF(v); /* Synonym: owner email addr. */

    v = Py_BuildValue("i", CRYPT_KEYID_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYID_LAST", v);
    Py_DECREF(v); /* Last possible key ID type */

    v = Py_BuildValue("i", CRYPT_OBJECT_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_OBJECT_NONE", v);
    Py_DECREF(v); /* No object type */

    v = Py_BuildValue("i", CRYPT_OBJECT_ENCRYPTED_KEY);
    PyDict_SetItemString(moduleDict, "CRYPT_OBJECT_ENCRYPTED_KEY", v);
    Py_DECREF(v); /* Conventionally encrypted key */

    v = Py_BuildValue("i", CRYPT_OBJECT_PKCENCRYPTED_KEY);
    PyDict_SetItemString(moduleDict, "CRYPT_OBJECT_PKCENCRYPTED_KEY", v);
    Py_DECREF(v); /* PKC-encrypted key */

    v = Py_BuildValue("i", CRYPT_OBJECT_KEYAGREEMENT);
    PyDict_SetItemString(moduleDict, "CRYPT_OBJECT_KEYAGREEMENT", v);
    Py_DECREF(v); /* Key agreement information */

    v = Py_BuildValue("i", CRYPT_OBJECT_SIGNATURE);
    PyDict_SetItemString(moduleDict, "CRYPT_OBJECT_SIGNATURE", v);
    Py_DECREF(v); /* Signature */

    v = Py_BuildValue("i", CRYPT_OBJECT_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_OBJECT_LAST", v);
    Py_DECREF(v); /* Last possible object type */

    v = Py_BuildValue("i", CRYPT_ERRTYPE_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_ERRTYPE_NONE", v);
    Py_DECREF(v); /* No error information */

    v = Py_BuildValue("i", CRYPT_ERRTYPE_ATTR_SIZE);
    PyDict_SetItemString(moduleDict, "CRYPT_ERRTYPE_ATTR_SIZE", v);
    Py_DECREF(v); /* Attribute data too small or large */

    v = Py_BuildValue("i", CRYPT_ERRTYPE_ATTR_VALUE);
    PyDict_SetItemString(moduleDict, "CRYPT_ERRTYPE_ATTR_VALUE", v);
    Py_DECREF(v); /* Attribute value is invalid */

    v = Py_BuildValue("i", CRYPT_ERRTYPE_ATTR_ABSENT);
    PyDict_SetItemString(moduleDict, "CRYPT_ERRTYPE_ATTR_ABSENT", v);
    Py_DECREF(v); /* Required attribute missing */

    v = Py_BuildValue("i", CRYPT_ERRTYPE_ATTR_PRESENT);
    PyDict_SetItemString(moduleDict, "CRYPT_ERRTYPE_ATTR_PRESENT", v);
    Py_DECREF(v); /* Non-allowed attribute present */

    v = Py_BuildValue("i", CRYPT_ERRTYPE_CONSTRAINT);
    PyDict_SetItemString(moduleDict, "CRYPT_ERRTYPE_CONSTRAINT", v);
    Py_DECREF(v); /* Cert: Constraint violation in object */

    v = Py_BuildValue("i", CRYPT_ERRTYPE_ISSUERCONSTRAINT);
    PyDict_SetItemString(moduleDict, "CRYPT_ERRTYPE_ISSUERCONSTRAINT", v);
    Py_DECREF(v); /* Cert: Constraint viol.in issuing cert */

    v = Py_BuildValue("i", CRYPT_ERRTYPE_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_ERRTYPE_LAST", v);
    Py_DECREF(v); /* Last possible error info type */

    v = Py_BuildValue("i", CRYPT_CERTACTION_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_NONE", v);
    Py_DECREF(v); /* No cert management action */

    v = Py_BuildValue("i", CRYPT_CERTACTION_CREATE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_CREATE", v);
    Py_DECREF(v); /* Create cert store */

    v = Py_BuildValue("i", CRYPT_CERTACTION_CONNECT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_CONNECT", v);
    Py_DECREF(v); /* Connect to cert store */

    v = Py_BuildValue("i", CRYPT_CERTACTION_DISCONNECT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_DISCONNECT", v);
    Py_DECREF(v); /* Disconnect from cert store */

    v = Py_BuildValue("i", CRYPT_CERTACTION_ERROR);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_ERROR", v);
    Py_DECREF(v); /* Error information */

    v = Py_BuildValue("i", CRYPT_CERTACTION_ADDUSER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_ADDUSER", v);
    Py_DECREF(v); /* Add PKI user */

    v = Py_BuildValue("i", CRYPT_CERTACTION_DELETEUSER);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_DELETEUSER", v);
    Py_DECREF(v); /* Delete PKI user */

    v = Py_BuildValue("i", CRYPT_CERTACTION_REQUEST_CERT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_REQUEST_CERT", v);
    Py_DECREF(v); /* Cert request */

    v = Py_BuildValue("i", CRYPT_CERTACTION_REQUEST_RENEWAL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_REQUEST_RENEWAL", v);
    Py_DECREF(v); /* Cert renewal request */

    v = Py_BuildValue("i", CRYPT_CERTACTION_REQUEST_REVOCATION);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_REQUEST_REVOCATION", v);
    Py_DECREF(v); /* Cert revocation request */

    v = Py_BuildValue("i", CRYPT_CERTACTION_CERT_CREATION);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_CERT_CREATION", v);
    Py_DECREF(v); /* Cert creation */

    v = Py_BuildValue("i", CRYPT_CERTACTION_CERT_CREATION_COMPLETE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_CERT_CREATION_COMPLETE", v);
    Py_DECREF(v); /* Confirmation of cert creation */

    v = Py_BuildValue("i", CRYPT_CERTACTION_CERT_CREATION_DROP);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_CERT_CREATION_DROP", v);
    Py_DECREF(v); /* Cancellation of cert creation */

    v = Py_BuildValue("i", CRYPT_CERTACTION_CERT_CREATION_REVERSE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_CERT_CREATION_REVERSE", v);
    Py_DECREF(v); /* Cancel of creation w.revocation */

    v = Py_BuildValue("i", CRYPT_CERTACTION_RESTART_CLEANUP);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_RESTART_CLEANUP", v);
    Py_DECREF(v); /* Delete reqs after restart */

    v = Py_BuildValue("i", CRYPT_CERTACTION_RESTART_REVOKE_CERT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_RESTART_REVOKE_CERT", v);
    Py_DECREF(v); /* Complete revocation after restart */

    v = Py_BuildValue("i", CRYPT_CERTACTION_ISSUE_CERT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_ISSUE_CERT", v);
    Py_DECREF(v); /* Cert issue */

    v = Py_BuildValue("i", CRYPT_CERTACTION_ISSUE_CRL);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_ISSUE_CRL", v);
    Py_DECREF(v); /* CRL issue */

    v = Py_BuildValue("i", CRYPT_CERTACTION_REVOKE_CERT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_REVOKE_CERT", v);
    Py_DECREF(v); /* Cert revocation */

    v = Py_BuildValue("i", CRYPT_CERTACTION_EXPIRE_CERT);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_EXPIRE_CERT", v);
    Py_DECREF(v); /* Cert expiry */

    v = Py_BuildValue("i", CRYPT_CERTACTION_CLEANUP);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_CLEANUP", v);
    Py_DECREF(v); /* Clean up on restart */

    v = Py_BuildValue("i", CRYPT_CERTACTION_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTACTION_LAST", v);
    Py_DECREF(v); /* Last possible cert store log action */

    v = Py_BuildValue("i", CRYPT_KEYOPT_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYOPT_NONE", v);
    Py_DECREF(v); /* No options */

    v = Py_BuildValue("i", CRYPT_KEYOPT_READONLY);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYOPT_READONLY", v);
    Py_DECREF(v); /* Open keyset in read-only mode */

    v = Py_BuildValue("i", CRYPT_KEYOPT_CREATE);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYOPT_CREATE", v);
    Py_DECREF(v); /* Create a new keyset */

    v = Py_BuildValue("i", CRYPT_KEYOPT_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYOPT_LAST", v);
    Py_DECREF(v); /* Last possible key option type */

    v = Py_BuildValue("i", CRYPT_ECCCURVE_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_ECCCURVE_NONE", v);
    Py_DECREF(v); /* No ECC curve type */

    v = Py_BuildValue("i", CRYPT_ECCCURVE_P256);
    PyDict_SetItemString(moduleDict, "CRYPT_ECCCURVE_P256", v);
    Py_DECREF(v); /* NIST P256/X9.62 P256v1/SECG p256r1 curve */

    v = Py_BuildValue("i", CRYPT_ECCCURVE_P384);
    PyDict_SetItemString(moduleDict, "CRYPT_ECCCURVE_P384", v);
    Py_DECREF(v); /* NIST P384, SECG p384r1 curve */

    v = Py_BuildValue("i", CRYPT_ECCCURVE_P521);
    PyDict_SetItemString(moduleDict, "CRYPT_ECCCURVE_P521", v);
    Py_DECREF(v); /* NIST P521, SECG p521r1 */

    v = Py_BuildValue("i", CRYPT_ECCCURVE_BRAINPOOL_P256);
    PyDict_SetItemString(moduleDict, "CRYPT_ECCCURVE_BRAINPOOL_P256", v);
    Py_DECREF(v); /* Brainpool p256r1 */

    v = Py_BuildValue("i", CRYPT_ECCCURVE_BRAINPOOL_P384);
    PyDict_SetItemString(moduleDict, "CRYPT_ECCCURVE_BRAINPOOL_P384", v);
    Py_DECREF(v); /* Brainpool p384r1 */

    v = Py_BuildValue("i", CRYPT_ECCCURVE_BRAINPOOL_P512);
    PyDict_SetItemString(moduleDict, "CRYPT_ECCCURVE_BRAINPOOL_P512", v);
    Py_DECREF(v); /* Brainpool p512r1 */

    v = Py_BuildValue("i", CRYPT_ECCCURVE_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_ECCCURVE_LAST", v);
    Py_DECREF(v); /* Last valid ECC curve type */

    v = Py_BuildValue("i", CRYPT_CRLREASON_UNSPECIFIED);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASON_UNSPECIFIED", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CRLREASON_KEYCOMPROMISE);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASON_KEYCOMPROMISE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CRLREASON_CACOMPROMISE);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASON_CACOMPROMISE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CRLREASON_AFFILIATIONCHANGED);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASON_AFFILIATIONCHANGED", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CRLREASON_SUPERSEDED);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASON_SUPERSEDED", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CRLREASON_CESSATIONOFOPERATION);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASON_CESSATIONOFOPERATION", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CRLREASON_CERTIFICATEHOLD);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASON_CERTIFICATEHOLD", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CRLREASON_REMOVEFROMCRL);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASON_REMOVEFROMCRL", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CRLREASON_PRIVILEGEWITHDRAWN);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASON_PRIVILEGEWITHDRAWN", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CRLREASON_AACOMPROMISE);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASON_AACOMPROMISE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CRLREASON_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASON_LAST", v);
    Py_DECREF(v); /* End of standard CRL reasons */

    v = Py_BuildValue("i", CRYPT_CRLREASON_NEVERVALID);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASON_NEVERVALID", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CRLEXTREASON_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLEXTREASON_LAST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_HOLDINSTRUCTION_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_HOLDINSTRUCTION_NONE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_HOLDINSTRUCTION_CALLISSUER);
    PyDict_SetItemString(moduleDict, "CRYPT_HOLDINSTRUCTION_CALLISSUER", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_HOLDINSTRUCTION_REJECT);
    PyDict_SetItemString(moduleDict, "CRYPT_HOLDINSTRUCTION_REJECT", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_HOLDINSTRUCTION_PICKUPTOKEN);
    PyDict_SetItemString(moduleDict, "CRYPT_HOLDINSTRUCTION_PICKUPTOKEN", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_HOLDINSTRUCTION_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_HOLDINSTRUCTION_LAST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_COMPLIANCELEVEL_OBLIVIOUS);
    PyDict_SetItemString(moduleDict, "CRYPT_COMPLIANCELEVEL_OBLIVIOUS", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_COMPLIANCELEVEL_REDUCED);
    PyDict_SetItemString(moduleDict, "CRYPT_COMPLIANCELEVEL_REDUCED", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_COMPLIANCELEVEL_STANDARD);
    PyDict_SetItemString(moduleDict, "CRYPT_COMPLIANCELEVEL_STANDARD", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL);
    PyDict_SetItemString(moduleDict, "CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_COMPLIANCELEVEL_PKIX_FULL);
    PyDict_SetItemString(moduleDict, "CRYPT_COMPLIANCELEVEL_PKIX_FULL", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_COMPLIANCELEVEL_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_COMPLIANCELEVEL_LAST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CLASSIFICATION_UNMARKED);
    PyDict_SetItemString(moduleDict, "CRYPT_CLASSIFICATION_UNMARKED", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CLASSIFICATION_UNCLASSIFIED);
    PyDict_SetItemString(moduleDict, "CRYPT_CLASSIFICATION_UNCLASSIFIED", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CLASSIFICATION_RESTRICTED);
    PyDict_SetItemString(moduleDict, "CRYPT_CLASSIFICATION_RESTRICTED", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CLASSIFICATION_CONFIDENTIAL);
    PyDict_SetItemString(moduleDict, "CRYPT_CLASSIFICATION_CONFIDENTIAL", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CLASSIFICATION_SECRET);
    PyDict_SetItemString(moduleDict, "CRYPT_CLASSIFICATION_SECRET", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CLASSIFICATION_TOP_SECRET);
    PyDict_SetItemString(moduleDict, "CRYPT_CLASSIFICATION_TOP_SECRET", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CLASSIFICATION_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_CLASSIFICATION_LAST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTSTATUS_VALID);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTSTATUS_VALID", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTSTATUS_NOTVALID);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTSTATUS_NOTVALID", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTSTATUS_NONAUTHORITATIVE);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTSTATUS_NONAUTHORITATIVE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CERTSTATUS_UNKNOWN);
    PyDict_SetItemString(moduleDict, "CRYPT_CERTSTATUS_UNKNOWN", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_OCSPSTATUS_NOTREVOKED);
    PyDict_SetItemString(moduleDict, "CRYPT_OCSPSTATUS_NOTREVOKED", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_OCSPSTATUS_REVOKED);
    PyDict_SetItemString(moduleDict, "CRYPT_OCSPSTATUS_REVOKED", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_OCSPSTATUS_UNKNOWN);
    PyDict_SetItemString(moduleDict, "CRYPT_OCSPSTATUS_UNKNOWN", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_KEYUSAGE_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYUSAGE_NONE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_KEYUSAGE_DIGITALSIGNATURE);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYUSAGE_DIGITALSIGNATURE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_KEYUSAGE_NONREPUDIATION);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYUSAGE_NONREPUDIATION", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_KEYUSAGE_KEYENCIPHERMENT);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYUSAGE_KEYENCIPHERMENT", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_KEYUSAGE_DATAENCIPHERMENT);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYUSAGE_DATAENCIPHERMENT", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_KEYUSAGE_KEYAGREEMENT);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYUSAGE_KEYAGREEMENT", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_KEYUSAGE_KEYCERTSIGN);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYUSAGE_KEYCERTSIGN", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_KEYUSAGE_CRLSIGN);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYUSAGE_CRLSIGN", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_KEYUSAGE_ENCIPHERONLY);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYUSAGE_ENCIPHERONLY", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_KEYUSAGE_DECIPHERONLY);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYUSAGE_DECIPHERONLY", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_KEYUSAGE_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYUSAGE_LAST", v);
    Py_DECREF(v); /* Last possible value */

    v = Py_BuildValue("i", CRYPT_CRLREASONFLAG_UNUSED);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASONFLAG_UNUSED", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CRLREASONFLAG_KEYCOMPROMISE);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASONFLAG_KEYCOMPROMISE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CRLREASONFLAG_CACOMPROMISE);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASONFLAG_CACOMPROMISE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CRLREASONFLAG_AFFILIATIONCHANGED);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASONFLAG_AFFILIATIONCHANGED", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CRLREASONFLAG_SUPERSEDED);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASONFLAG_SUPERSEDED", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CRLREASONFLAG_CESSATIONOFOPERATION);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASONFLAG_CESSATIONOFOPERATION", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CRLREASONFLAG_CERTIFICATEHOLD);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASONFLAG_CERTIFICATEHOLD", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CRLREASONFLAG_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_CRLREASONFLAG_LAST", v);
    Py_DECREF(v); /* Last poss.value */

    v = Py_BuildValue("i", CRYPT_NS_CERTTYPE_SSLCLIENT);
    PyDict_SetItemString(moduleDict, "CRYPT_NS_CERTTYPE_SSLCLIENT", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_NS_CERTTYPE_SSLSERVER);
    PyDict_SetItemString(moduleDict, "CRYPT_NS_CERTTYPE_SSLSERVER", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_NS_CERTTYPE_SMIME);
    PyDict_SetItemString(moduleDict, "CRYPT_NS_CERTTYPE_SMIME", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_NS_CERTTYPE_OBJECTSIGNING);
    PyDict_SetItemString(moduleDict, "CRYPT_NS_CERTTYPE_OBJECTSIGNING", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_NS_CERTTYPE_RESERVED);
    PyDict_SetItemString(moduleDict, "CRYPT_NS_CERTTYPE_RESERVED", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_NS_CERTTYPE_SSLCA);
    PyDict_SetItemString(moduleDict, "CRYPT_NS_CERTTYPE_SSLCA", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_NS_CERTTYPE_SMIMECA);
    PyDict_SetItemString(moduleDict, "CRYPT_NS_CERTTYPE_SMIMECA", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_NS_CERTTYPE_OBJECTSIGNINGCA);
    PyDict_SetItemString(moduleDict, "CRYPT_NS_CERTTYPE_OBJECTSIGNINGCA", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_NS_CERTTYPE_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_NS_CERTTYPE_LAST", v);
    Py_DECREF(v); /* Last possible value */

    v = Py_BuildValue("i", CRYPT_SET_CERTTYPE_CARD);
    PyDict_SetItemString(moduleDict, "CRYPT_SET_CERTTYPE_CARD", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_SET_CERTTYPE_MER);
    PyDict_SetItemString(moduleDict, "CRYPT_SET_CERTTYPE_MER", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_SET_CERTTYPE_PGWY);
    PyDict_SetItemString(moduleDict, "CRYPT_SET_CERTTYPE_PGWY", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_SET_CERTTYPE_CCA);
    PyDict_SetItemString(moduleDict, "CRYPT_SET_CERTTYPE_CCA", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_SET_CERTTYPE_MCA);
    PyDict_SetItemString(moduleDict, "CRYPT_SET_CERTTYPE_MCA", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_SET_CERTTYPE_PCA);
    PyDict_SetItemString(moduleDict, "CRYPT_SET_CERTTYPE_PCA", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_SET_CERTTYPE_GCA);
    PyDict_SetItemString(moduleDict, "CRYPT_SET_CERTTYPE_GCA", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_SET_CERTTYPE_BCA);
    PyDict_SetItemString(moduleDict, "CRYPT_SET_CERTTYPE_BCA", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_SET_CERTTYPE_RCA);
    PyDict_SetItemString(moduleDict, "CRYPT_SET_CERTTYPE_RCA", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_SET_CERTTYPE_ACQ);
    PyDict_SetItemString(moduleDict, "CRYPT_SET_CERTTYPE_ACQ", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_SET_CERTTYPE_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_SET_CERTTYPE_LAST", v);
    Py_DECREF(v); /* Last possible value */

    v = Py_BuildValue("i", CRYPT_SSLOPTION_NONE);
    PyDict_SetItemString(moduleDict, "CRYPT_SSLOPTION_NONE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_SSLOPTION_MINVER_SSLV3);
    PyDict_SetItemString(moduleDict, "CRYPT_SSLOPTION_MINVER_SSLV3", v);
    Py_DECREF(v); /* Min.protocol version */

    v = Py_BuildValue("i", CRYPT_SSLOPTION_MINVER_TLS10);
    PyDict_SetItemString(moduleDict, "CRYPT_SSLOPTION_MINVER_TLS10", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_SSLOPTION_MINVER_TLS11);
    PyDict_SetItemString(moduleDict, "CRYPT_SSLOPTION_MINVER_TLS11", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_SSLOPTION_MINVER_TLS12);
    PyDict_SetItemString(moduleDict, "CRYPT_SSLOPTION_MINVER_TLS12", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_SSLOPTION_MINVER_TLS13);
    PyDict_SetItemString(moduleDict, "CRYPT_SSLOPTION_MINVER_TLS13", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_SSLOPTION_MANUAL_CERTCHECK);
    PyDict_SetItemString(moduleDict, "CRYPT_SSLOPTION_MANUAL_CERTCHECK", v);
    Py_DECREF(v); /* Require manual cert.verif. */

    v = Py_BuildValue("i", CRYPT_SSLOPTION_DISABLE_NAMEVERIFY);
    PyDict_SetItemString(moduleDict, "CRYPT_SSLOPTION_DISABLE_NAMEVERIFY", v);
    Py_DECREF(v); /* Disable cert hostname check */

    v = Py_BuildValue("i", CRYPT_SSLOPTION_DISABLE_CERTVERIFY);
    PyDict_SetItemString(moduleDict, "CRYPT_SSLOPTION_DISABLE_CERTVERIFY", v);
    Py_DECREF(v); /* Disable certificate check */

    v = Py_BuildValue("i", CRYPT_SSLOPTION_SUITEB_128);
    PyDict_SetItemString(moduleDict, "CRYPT_SSLOPTION_SUITEB_128", v);
    Py_DECREF(v); /* SuiteB security levels (may */

    v = Py_BuildValue("i", CRYPT_SSLOPTION_SUITEB_256);
    PyDict_SetItemString(moduleDict, "CRYPT_SSLOPTION_SUITEB_256", v);
    Py_DECREF(v); /* vanish in future releases) */

    v = Py_BuildValue("i", CRYPT_MAX_KEYSIZE);
    PyDict_SetItemString(moduleDict, "CRYPT_MAX_KEYSIZE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_MAX_IVSIZE);
    PyDict_SetItemString(moduleDict, "CRYPT_MAX_IVSIZE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_MAX_PKCSIZE);
    PyDict_SetItemString(moduleDict, "CRYPT_MAX_PKCSIZE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_MAX_PKCSIZE_ECC);
    PyDict_SetItemString(moduleDict, "CRYPT_MAX_PKCSIZE_ECC", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_MAX_HASHSIZE);
    PyDict_SetItemString(moduleDict, "CRYPT_MAX_HASHSIZE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_MAX_TEXTSIZE);
    PyDict_SetItemString(moduleDict, "CRYPT_MAX_TEXTSIZE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_KEYTYPE_PRIVATE);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYTYPE_PRIVATE", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_KEYTYPE_PUBLIC);
    PyDict_SetItemString(moduleDict, "CRYPT_KEYTYPE_PUBLIC", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_OK);
    PyDict_SetItemString(moduleDict, "CRYPT_OK", v);
    Py_DECREF(v); /* No error */

    v = Py_BuildValue("i", CRYPT_USE_DEFAULT);
    PyDict_SetItemString(moduleDict, "CRYPT_USE_DEFAULT", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_UNUSED);
    PyDict_SetItemString(moduleDict, "CRYPT_UNUSED", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CURSOR_FIRST);
    PyDict_SetItemString(moduleDict, "CRYPT_CURSOR_FIRST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CURSOR_PREVIOUS);
    PyDict_SetItemString(moduleDict, "CRYPT_CURSOR_PREVIOUS", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CURSOR_NEXT);
    PyDict_SetItemString(moduleDict, "CRYPT_CURSOR_NEXT", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_CURSOR_LAST);
    PyDict_SetItemString(moduleDict, "CRYPT_CURSOR_LAST", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_RANDOM_FASTPOLL);
    PyDict_SetItemString(moduleDict, "CRYPT_RANDOM_FASTPOLL", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_RANDOM_SLOWPOLL);
    PyDict_SetItemString(moduleDict, "CRYPT_RANDOM_SLOWPOLL", v);
    Py_DECREF(v);

    v = Py_BuildValue("i", CRYPT_ERROR_PARAM1);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_PARAM1", v);
    Py_DECREF(v); /* Bad argument, parameter 1 */

    v = Py_BuildValue("i", CRYPT_ERROR_PARAM2);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_PARAM2", v);
    Py_DECREF(v); /* Bad argument, parameter 2 */

    v = Py_BuildValue("i", CRYPT_ERROR_PARAM3);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_PARAM3", v);
    Py_DECREF(v); /* Bad argument, parameter 3 */

    v = Py_BuildValue("i", CRYPT_ERROR_PARAM4);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_PARAM4", v);
    Py_DECREF(v); /* Bad argument, parameter 4 */

    v = Py_BuildValue("i", CRYPT_ERROR_PARAM5);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_PARAM5", v);
    Py_DECREF(v); /* Bad argument, parameter 5 */

    v = Py_BuildValue("i", CRYPT_ERROR_PARAM6);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_PARAM6", v);
    Py_DECREF(v); /* Bad argument, parameter 6 */

    v = Py_BuildValue("i", CRYPT_ERROR_PARAM7);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_PARAM7", v);
    Py_DECREF(v); /* Bad argument, parameter 7 */

    v = Py_BuildValue("i", CRYPT_ERROR_MEMORY);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_MEMORY", v);
    Py_DECREF(v); /* Out of memory */

    v = Py_BuildValue("i", CRYPT_ERROR_NOTINITED);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_NOTINITED", v);
    Py_DECREF(v); /* Data has not been initialised */

    v = Py_BuildValue("i", CRYPT_ERROR_INITED);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_INITED", v);
    Py_DECREF(v); /* Data has already been init'd */

    v = Py_BuildValue("i", CRYPT_ERROR_NOSECURE);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_NOSECURE", v);
    Py_DECREF(v); /* Opn.not avail.at requested sec.level */

    v = Py_BuildValue("i", CRYPT_ERROR_RANDOM);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_RANDOM", v);
    Py_DECREF(v); /* No reliable random data available */

    v = Py_BuildValue("i", CRYPT_ERROR_FAILED);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_FAILED", v);
    Py_DECREF(v); /* Operation failed */

    v = Py_BuildValue("i", CRYPT_ERROR_INTERNAL);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_INTERNAL", v);
    Py_DECREF(v); /* Internal consistency check failed */

    v = Py_BuildValue("i", CRYPT_ERROR_NOTAVAIL);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_NOTAVAIL", v);
    Py_DECREF(v); /* This type of opn.not available */

    v = Py_BuildValue("i", CRYPT_ERROR_PERMISSION);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_PERMISSION", v);
    Py_DECREF(v); /* No permiss.to perform this operation */

    v = Py_BuildValue("i", CRYPT_ERROR_WRONGKEY);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_WRONGKEY", v);
    Py_DECREF(v); /* Incorrect key used to decrypt data */

    v = Py_BuildValue("i", CRYPT_ERROR_INCOMPLETE);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_INCOMPLETE", v);
    Py_DECREF(v); /* Operation incomplete/still in progress */

    v = Py_BuildValue("i", CRYPT_ERROR_COMPLETE);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_COMPLETE", v);
    Py_DECREF(v); /* Operation complete/can't continue */

    v = Py_BuildValue("i", CRYPT_ERROR_TIMEOUT);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_TIMEOUT", v);
    Py_DECREF(v); /* Operation timed out before completion */

    v = Py_BuildValue("i", CRYPT_ERROR_INVALID);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_INVALID", v);
    Py_DECREF(v); /* Invalid/inconsistent information */

    v = Py_BuildValue("i", CRYPT_ERROR_SIGNALLED);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_SIGNALLED", v);
    Py_DECREF(v); /* Resource destroyed by extnl.event */

    v = Py_BuildValue("i", CRYPT_ERROR_OVERFLOW);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_OVERFLOW", v);
    Py_DECREF(v); /* Resources/space exhausted */

    v = Py_BuildValue("i", CRYPT_ERROR_UNDERFLOW);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_UNDERFLOW", v);
    Py_DECREF(v); /* Not enough data available */

    v = Py_BuildValue("i", CRYPT_ERROR_BADDATA);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_BADDATA", v);
    Py_DECREF(v); /* Bad/unrecognised data format */

    v = Py_BuildValue("i", CRYPT_ERROR_SIGNATURE);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_SIGNATURE", v);
    Py_DECREF(v); /* Signature/integrity check failed */

    v = Py_BuildValue("i", CRYPT_ERROR_OPEN);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_OPEN", v);
    Py_DECREF(v); /* Cannot open object */

    v = Py_BuildValue("i", CRYPT_ERROR_READ);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_READ", v);
    Py_DECREF(v); /* Cannot read item from object */

    v = Py_BuildValue("i", CRYPT_ERROR_WRITE);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_WRITE", v);
    Py_DECREF(v); /* Cannot write item to object */

    v = Py_BuildValue("i", CRYPT_ERROR_NOTFOUND);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_NOTFOUND", v);
    Py_DECREF(v); /* Requested item not found in object */

    v = Py_BuildValue("i", CRYPT_ERROR_DUPLICATE);
    PyDict_SetItemString(moduleDict, "CRYPT_ERROR_DUPLICATE", v);
    Py_DECREF(v); /* Item already present in object */

    v = Py_BuildValue("i", CRYPT_ENVELOPE_RESOURCE);
    PyDict_SetItemString(moduleDict, "CRYPT_ENVELOPE_RESOURCE", v);
    Py_DECREF(v); /* Need resource to proceed */
}