; cryptlib Library Definition File for PalmOS
;
; We need at least PalmOS 5 (more likely 6) to run.

OSVERSION	5
ARMARCH		0
REVISION	3
RESOURCEID	0

; Library type (shared lib) and creator

TYPE		slib
CREATOR		clib

; Shared libraries must have one library entry function.

ENTRY		cryptlibMain

; Exported functions.  Note that the blank line after the EXPORTS
; keyword is required.

EXPORTS
			cryptAddCertExtension
			cryptAddPrivateKey
			cryptAddPublicKey
			cryptAddRandom
			cryptAsyncCancel
			cryptAsyncQuery
			cryptCAAddItem
			cryptCACertManagement
			cryptCADeleteItem
			cryptCAGetItem
			cryptCheckCert
			cryptCheckSignature
			cryptCheckSignatureEx
			cryptCreateCert
			cryptCreateContext
			cryptCreateEnvelope
			cryptCreateSession
			cryptCreateSignature
			cryptCreateSignatureEx
			cryptDecrypt
			cryptDeleteAttribute
			cryptDeleteCertExtension
			cryptDeleteKey
			cryptDestroyCert
			cryptDestroyContext
			cryptDestroyEnvelope
			cryptDestroyObject
			cryptDestroySession
			cryptDeviceClose
			cryptDeviceCreateContext
			cryptDeviceOpen
			cryptDeviceQueryCapability
			cryptEncrypt
			cryptEnd
			cryptExportCert
			cryptExportKey
			cryptExportKeyEx
			cryptFlushData
			cryptGenerateKey
			cryptGenerateKeyAsync
			cryptGetAttribute
			cryptGetAttributeString
			cryptGetCertExtension
			cryptGetKey
			cryptGetPrivateKey
			cryptGetPublicKey
			cryptImportCert
			cryptImportKey
			cryptImportKeyEx
			cryptInit
			cryptKeysetClose
			cryptKeysetOpen
			cryptLogin
			cryptLogout
			cryptPopData
			cryptPushData
			cryptQueryCapability
			cryptQueryObject
			cryptSetAttribute
			cryptSetAttributeString
			cryptSignCert
