#
#   Cryptkit - Tcl interface to Cryptlib Encryption Toolkit
#   (http://www.cs.auckland.ac.nz/~pgut001/cryptlib/)
#
#   Development by Steve Landers <steve@DigitalSmarties.com>
#   
#   $Id: cryptkit.tcl,v 1.28 2004/12/07 02:15:07 steve Exp $

package provide cryptkit 1.0

if {[catch {package require critcl}]} {
    puts stderr "This extension requires Critcl to be compiled"
    exit 1
}

if {![critcl::compiling]} {
    puts stderr "Critcl can't compile on this platform"
    exit 1
}

namespace eval cryptkit {

    namespace export crypt*

    critcl::cheaders -I[pwd]

    critcl::clibraries -L[pwd] -lcl_[critcl::platform]

    # platform specific stuff
    switch [critcl::platform] {
        Linux-x86 {
            critcl::clibraries -lresolv
        }
    }

    # force this source file into the resulting package so that the
    # pure Tcl procedures are available at run time
    critcl::tsources [file tail [info script]]

    # map Cryptlib #defines and enums into the current namespace
    critcl::cdefines CRYPT_* [namespace current]

    # other defines
    critcl::cdefines {
        NULL
        TRUE
        FALSE
        TCL_OK
        TCL_ERROR
    } [namespace current]
        
    critcl::ccode {
        #include <string.h>
        #include "cryptlib.h"

        int
        GetIntOrConstArg(Tcl_Interp *ip, Tcl_Obj *obj, int *val) {
            Tcl_Obj *obj1;
            if (Tcl_GetIntFromObj(NULL, obj, val) != TCL_OK) {
                Tcl_Obj *constObj = Tcl_NewStringObj("::cryptkit::",-1);
                Tcl_AppendObjToObj(constObj, obj);
                Tcl_IncrRefCount(constObj);
                if ((obj1 = Tcl_ObjGetVar2(ip, constObj, NULL,
                         TCL_LEAVE_ERR_MSG)) == NULL) {
                    Tcl_DecrRefCount(constObj);
                    return TCL_ERROR;
                }
                Tcl_DecrRefCount(constObj);
                if (Tcl_GetIntFromObj(ip, obj1, val) != TCL_OK) {
                    Tcl_AddErrorInfo(ip, " in '");
                    Tcl_AddErrorInfo(ip, Tcl_GetString(obj));
                    return TCL_ERROR;
                }
            }
            return TCL_OK;
        }

        int
        GetIntArg(Tcl_Interp *ip, Tcl_Obj *obj, int *val) {
            if (Tcl_GetIntFromObj(ip, obj, val) != TCL_OK) {
                Tcl_AddErrorInfo(ip, " in '");
                Tcl_AddErrorInfo(ip, Tcl_GetString(obj));
                return TCL_ERROR;
            }
            return TCL_OK;
        }

        int
        GetStringArg(Tcl_Interp *ip, Tcl_Obj *obj, char **val) {
            if ((*val = Tcl_GetStringFromObj(obj, NULL)) == NULL) {
                Tcl_AddErrorInfo(ip, " in '");
                Tcl_AddErrorInfo(ip, Tcl_GetString(obj));
                return TCL_ERROR;
            }
            return TCL_OK;
        }

        int
        GetByteArrayArg(Tcl_Interp *ip, Tcl_Obj *obj, void **val, int *len) {
            if (strcmp(Tcl_GetString(obj), "NULL") == 0) {
                *val = NULL;
                *len = 0;
            } else if ((*val = Tcl_GetByteArrayFromObj(obj, len)) == NULL) {
                Tcl_AddErrorInfo(ip, " in '");
                Tcl_AddErrorInfo(ip, Tcl_GetString(obj));
                return TCL_ERROR;
            }
            return TCL_OK;
        }

        void
        cryptSetQueryInfo(Tcl_Interp *ip, Tcl_Obj *info, \
                                         CRYPT_QUERY_INFO qinfo) {
            Tcl_ObjSetVar2(ip, info, Tcl_NewStringObj("algoName", -1), \
                                     Tcl_NewStringObj(qinfo.algoName, -1),
                                     TCL_LEAVE_ERR_MSG);
            Tcl_ObjSetVar2(ip, info, Tcl_NewStringObj("blockSize", -1), \
                                     Tcl_NewIntObj(qinfo.blockSize),
                                     TCL_LEAVE_ERR_MSG);
            Tcl_ObjSetVar2(ip, info, Tcl_NewStringObj("minKeySize", -1), \
                                     Tcl_NewIntObj(qinfo.minKeySize),
                                     TCL_LEAVE_ERR_MSG);
            Tcl_ObjSetVar2(ip, info, Tcl_NewStringObj("keySize", -1), \
                                     Tcl_NewIntObj(qinfo.keySize),
                                     TCL_LEAVE_ERR_MSG);
            Tcl_ObjSetVar2(ip, info, Tcl_NewStringObj("maxKeySize", -1), \
                                     Tcl_NewIntObj(qinfo.maxKeySize),
                                     TCL_LEAVE_ERR_MSG);
        }

        int
        cryptReturn(Tcl_Interp *ip, int ret) {
            Tcl_Obj *resultObj, *objv[2];

            objv[0] = Tcl_NewStringObj("::cryptkit::ReturnMsg", -1);
            objv[1] = Tcl_NewIntObj(ret);
            if (Tcl_EvalObjv(ip, 2, objv, TCL_EVAL_GLOBAL) != TCL_OK) {
                Tcl_AppendResult(ip, "oops", (char *) NULL);
                return TCL_ERROR;
            }
            resultObj = Tcl_GetObjResult(ip);
            if (Tcl_SetVar2Ex(ip, "::cryptkit::errmsg", NULL, resultObj,
                      TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG) == NULL) {
                return TCL_ERROR;
            }
            if (ret < CRYPT_OK) {
                Tcl_SetObjErrorCode(ip, resultObj);
                return TCL_ERROR;
            }
            return TCL_OK;
        }

    }

    # Return message (either number or text, or generate error)
    proc ReturnMsg {num} {
        variable errtype
        variable status
        if {![info exists errtype]} {
            # probably means cryptInit not called
            cryptInit
        }
        if {$errtype ne "text"} {
            return $num
        }
        if {[info exists status($num)]} {
            if {$num < 0} {
                return $status($num)
            } else {
                return $status($num)
            }
        } else {
            return "unknown Cryptlib error $num"
        }
    }

    # append to variable holding generated C code - makes code more readable
    # and allows it to be split over multiple lines
    proc code {args} {
        variable code
        set tmp ""
        foreach arg $args {
            append tmp $arg
        }
        lappend code $tmp
    }

    # generate critcl::ccomands to call C cryptlib functions
    proc generate {cmds} {
        if {[critcl::done]} {
            # we don't want to generate C code when the cryptkit package
            # is loaded at runtime
            return
        }
        variable code ""
        foreach {name arglist} $cmds {
            generate_function crypt$name [parse_arguments $arglist]
        }
        # uncomment the following line to view generated code
        # puts \n[join $code \n]
        eval [join $code \n]
    }

    proc parse_arguments {arglist} {
        set args [list]
        set argnum 0
        foreach arg $arglist {
            set suffix [string index $arg end]
            if {[string is alnum $suffix]} {
                set suffix ""
            } else {
                set arg [string range $arg 0 end-1]
            }
            lappend args [incr argnum] $arg $suffix
        }
        return $args
    }

    proc generate_function {name arguments} {
        # generate the critcl::cccomand
        variable allocated ""   ;# any Tcl_Alloc'd variables
        variable carglist ""    ;# list of arguments to the Cryptlib function
        variable numargs 0      ;# number of arguments to the Cryptlib function
        variable skiparg 0      ;# number of arguments skipped in Tcl API
        code "critcl::ccommand $name {data ip objc objv} \{"
        code "  int ret;"
        set objects ""      ;# Tcl objects to create
        set vararg 0        ;# indicates we have an option argument
        set usage ""        ;# usage message
        # generate definitions for arguments to Cryptlib function
        foreach {n arg suffix} $arguments {
            switch $suffix {
                *       { code "  char *arg$n;" }
                %       -
                :       -
                =       { code "  void *arg$n; int len$n;" }
                default { code "  int arg$n;" }
            }
            if {$suffix ne "#" && $suffix ne "!"} {
                lappend usage $arg
                lappend objects "*obj$n"
                incr numargs 
            }
        }
        # create Tcl objects for each argument to Cryptkit Tcl procedure
        if {$objects ne {}} {
            code "  Tcl_Obj [join $objects {, }];"
        }
        # check number of arguments
        code "  if (objc != [expr {$numargs + 1}]) \{"
        code "    Tcl_WrongNumArgs(ip, 1, objv, \"$usage\");"
        code "    return TCL_ERROR;"
        code "  \}"
        # first get all args that can be extracted from the Tcl argument list
        foreach {n arg suffix} $arguments {
            get_argument_values $name $n $arg $suffix
            if {$suffix eq "&"} {
                lappend carglist "&arg$n"
            } else {
                lappend carglist "arg$n"
            }
        }
        # now get any that are derived from later arguments
        foreach {n arg suffix} $arguments {
            if {$suffix eq "%"} {
                # get length of buffer from the next argument
                code "  arg$n = (void *) Tcl_Alloc(arg[expr {$n + 1}]);"
                lappend allocated arg$n
            }
        }
        query_return_buffer_size $name $arguments
        code "  ret = $name\([join $carglist {, }]\);" ;# call cryptlib function
        set_return_values $arguments
        foreach alloc $allocated {
            code "  Tcl_Free($alloc);"
        }
        code "  return cryptReturn(ip, ret);"
        code "\}"
        code ""
    }

    proc code_check {n name func} {
        variable skiparg
        set argnum [expr {$n - $skiparg}]
        code "  if ($func == TCL_ERROR) \{"
        code "    Tcl_AddErrorInfo(ip, \"' (argument $argnum to $name)\");" 
        code "    return TCL_ERROR;"
        code "  \}"
    }

    proc get_argument_values {name n arg suffix} {
        variable allocated
        variable numargs
        variable skiparg
        set r [expr {$n - $skiparg}]
        switch $suffix {
            & { # return integer value
              }
            ? { # if not specified set optional argument to length of
                # previous argument
                code "  if (objc != $numargs)"
                code "    arg$n = len[expr {$n - 1}];"
                code "  else"
                code_check $n $name "GetIntArg(ip, objv\[$r\], &arg$n)"
              }
            "#" { # use length of previous void *
                code "  arg$n = len[expr {$n - 1}];"
                incr skiparg
              }
            ! { # arbitrary value for maximum length of a void * 
                code "  arg$n = 32000;"
                incr skiparg
              }
            ^ { # integer or Cryptlib constant
                code_check $n $name "GetIntOrConstArg(ip, objv\[$r\], &arg$n)"
              }
            * { # char *
                code_check $n $name  "GetStringArg(ip, objv\[$r\], &arg$n)"
              }
            = { # void *
                  code_check $n $name \
                        "GetByteArrayArg(ip, objv\[$r\], &arg$n, &len$n)"
              }
            % { # handle specially }
            : { # we handle these specially in query_return_buffer_size
              }
            default { # integer
                 code_check $n $name "GetIntArg(ip, objv\[$r\], &arg$n)"
              }
        } 
    }

    # a number of cryptlib functions can be called with a NULL argument
    # to get the length of the data buffer  that will be returned
    proc query_return_buffer_size {name arguments} {
        variable allocated
        variable carglist
        # now look for candidates
        foreach {n arg suffix} $arguments {
            if {$suffix eq ":"} {
                set idx [expr {$n - 1}]
                set args [join [lreplace $carglist $idx $idx NULL] ,]
                code "  if \(ret = \($name\($args\)\) != CRYPT_OK\)"
                code "    return cryptReturn(ip, ret);"
                # need to check if next suffix is ! and ignore it
                set next [expr {($n+1)*3+2}]
                set x [expr {$n + 1}]
                while {[lindex $arguments $next] eq "!"} {
                    incr next 3
                    incr x
                }
                # create a buffer big enough to hold returned value
                code "  arg$n = (void *) Tcl_Alloc(arg$x);"
                lappend allocated arg$n
            }
        }
    }

    proc set_return_values {arguments} {
        set skip 0
        foreach {n arg suffix} $arguments {
            set r [expr {$n - $skip}]
            switch $suffix {
                % -
                : { # void *
                    code "  if (Tcl_ObjSetVar2(ip, objv\[$r\], NULL, " \
                                "Tcl_NewByteArrayObj(arg$n, " \
                                "arg[expr {$n+1}]), " \
                                "TCL_LEAVE_ERR_MSG) == NULL)"
                    code "    return TCL_ERROR;"
                  }
                & { # integer
                    code "  if (Tcl_ObjSetVar2(ip, objv\[$r\], NULL, " \
                                "Tcl_NewIntObj(arg$n)," \
                                "TCL_LEAVE_ERR_MSG) == NULL)"
                    code "    return TCL_ERROR;"
                  }
                ! -
                "#" { incr skip }
            }
        } 
    }

    proc mapping {name pattern} {
        variable $name
        foreach tmp [info vars [namespace current]::$pattern] {
            variable $tmp
            set var [namespace tail $tmp]
            set val [set $tmp]
            array set $name [list $val $var]
        }
    }

    # handle cryptInit specially, because we need to do some other setup as well
    proc cryptInit {{type text}} {
        # create a mapping from the C #defined errors to the textual
        # equivalent (note we introspect using [namespace current] since
        # namespace variables aren't visible unless explicitly declared
        mapping status CRYPT_ERROR_*
        mapping status CRYPT_OK
        mapping status CRYPT_ENVELOPE_RESOURCE
        # create a mapping from enums/defines for use in cryptQueryObject
        mapping objects CRYPT_ALGO_*
        mapping objects CRYPT_OBJECT_*
        mapping objects CRYPT_MODE_*
        variable errtype
        switch -glob $type {
            num* -
            text {
                set errtype $type
            }
            default {
                error "cryptInit: unknown error handling type '$type'"
            }
        }
        return [ReturnMsg [_cryptInit]]
    }

    critcl::cproc _cryptInit {} int {
        return cryptInit();
    }

    # functions that return structures are handled specially and the
    # structures are mapped into Tcl arrays
    #  DeviceQueryCapability { cryptDevice cryptAlgo &cryptQueryInfo }
    #  QueryCapability       { cryptAlgo &cryptQueryInfo }
    #  QueryObject           { objectPtr= &algocryptObjectInfo }

    critcl::cproc cryptDeviceQueryCapability \
            {Tcl_Interp* ip Tcl_Obj* dev Tcl_Obj* algo Tcl_Obj* info} ok {
        int ret, device, algorithm;
        CRYPT_QUERY_INFO qinfo;

        if (GetIntOrConstArg(ip, algo, &algorithm) != CRYPT_OK)
            return TCL_ERROR;
        if (GetIntOrConstArg(ip, dev, &device) != CRYPT_OK)
            return TCL_ERROR;
        if (GetIntOrConstArg(ip, algo, &algorithm) != CRYPT_OK)
            return TCL_ERROR;
        ret = cryptDeviceQueryCapability(device, algorithm, &qinfo);
        cryptSetQueryInfo(ip, info, qinfo);
        cryptReturn(ip, ret);
        return TCL_OK;
    }

    critcl::cproc cryptQueryCapability \
                {Tcl_Interp* ip Tcl_Obj* algo Tcl_Obj* info} ok {
        int ret, algorithm;
        CRYPT_QUERY_INFO qinfo;

        if (GetIntOrConstArg(ip, algo, &algorithm) != CRYPT_OK)
            return TCL_ERROR;
        ret = cryptQueryCapability(algorithm, &qinfo);
        cryptSetQueryInfo(ip, info, qinfo);
        cryptReturn(ip, ret);
        return TCL_OK;
    }

    critcl::cproc cryptQueryObject \
                {Tcl_Interp* ip Tcl_Obj* obj Tcl_Obj* info} ok {
        void *objbuf;
        CRYPT_OBJECT_INFO oinfo;
        int ret, len;
        Tcl_Obj *objects, *name;
        objbuf = Tcl_GetByteArrayFromObj(obj,&len);
        ret = cryptQueryObject(objbuf, len, &oinfo);
        if ((objects = Tcl_NewStringObj("::cryptkit::objects", -1)) == NULL)
            return TCL_ERROR;
        Tcl_ObjSetVar2(ip, info, Tcl_NewStringObj("objectType", -1), \
                Tcl_ObjGetVar2(ip, objects, Tcl_NewIntObj(oinfo.objectType),
                        TCL_LEAVE_ERR_MSG), TCL_LEAVE_ERR_MSG);
        Tcl_ObjSetVar2(ip, info, Tcl_NewStringObj("cryptAlgo", -1), \
                Tcl_ObjGetVar2(ip, objects, Tcl_NewIntObj(oinfo.cryptAlgo),
                        TCL_LEAVE_ERR_MSG), TCL_LEAVE_ERR_MSG);
        Tcl_ObjSetVar2(ip, info, Tcl_NewStringObj("cryptMode", -1), \
                Tcl_ObjGetVar2(ip, objects, Tcl_NewIntObj(oinfo.cryptMode),
                        TCL_LEAVE_ERR_MSG), TCL_LEAVE_ERR_MSG);
        Tcl_ObjSetVar2(ip, info, Tcl_NewStringObj("hashAlgo", -1), \
                Tcl_ObjGetVar2(ip, objects, Tcl_NewIntObj(oinfo.hashAlgo),
                        TCL_LEAVE_ERR_MSG), TCL_LEAVE_ERR_MSG);
        Tcl_ObjSetVar2(ip, info, Tcl_NewStringObj("salt", CRYPT_MAX_HASHSIZE), \
                                     Tcl_NewStringObj(oinfo.salt, -1),
                                     TCL_LEAVE_ERR_MSG);
        Tcl_ObjSetVar2(ip, info, Tcl_NewStringObj("saltSize", -1), \
                                     Tcl_NewIntObj(oinfo.saltSize),
                                     TCL_LEAVE_ERR_MSG);
        cryptReturn(ip, ret);
        return TCL_OK;
    }

    # generate remaining functions
    # function arguments
    #   default is "const int"
    # suffix modifiers to alter this
    #   ^   arg is name of #define/enum in cryptkit namespace
    #   *   char *
    #   =   void *
    #   &   return int
    #   %   return void *, next arg is length
    #   :   return void *, call with NULL to get length of string
    #   #   use length of previous void * (C API only)
    #   !   pass $maxlength to Cryptlib (C API only) 

    # Notes 
    #   - even though the GetPrivateKey documentation specifies the keyID
    #     should be "void *" it needed to be "char *" to work

    generate {
      AddCertExtension    { certificate oid* criticalFlag extension= \
                                extensionLength# }
      AddPrivateKey       { keyset cryptKey password* }
      AddPublicKey        { keyset certificate }
      AddRandom           { randomData= randomDataLength^ }
      AsyncCancel         { cryptObject }
      AsyncQuery          { cryptObject }
      CAAddItem           { keyset certificate }
      CACertManagement    { cryptCert& action keyset caKey certRequest }
      CAGetItem           { keyset certificate& certType keyIDtype keyID= }
      CheckCert           { certificate sigCheckKey }
      CheckSignature      { signature= length# sigCheckKey hashContext }
      CheckSignatureEx    { signature= length# sigCheckKey hashContext \
                                extraData& }
      CreateCert          { cryptCert& cryptUser^ certType^ }
      CreateContext       { cryptContext& cryptUser^ cryptAlgo^ }
      CreateEnvelope      { cryptEnvelope& cryptUser^ formatType^ }
      CreateSession       { cryptSession& cryptUser^ sessionType^ }
      CreateSignature     { signature: maxlength! signatureLength& \
                                signContext hashContext }
      CreateSignatureEx   { signature: maxlength! signatureLength& formatType \
                                signContext hashContext extraData }
      Decrypt             { cryptContext buffer= length# }
      DeleteAttribute     { cryptObject attributeType }
      DeleteCertExtension { certificate oid* }
      DeleteKey           { cryptObject keyIDtype keyID= }
      DestroyCert         { cryptCert }
      DestroyContext      { cryptContext }
      DestroyEnvelope     { cryptEnvelope }
      DestroyObject       { cryptObject }
      DestroySession      { cryptSession }
      DeviceClose         { device }
      DeviceCreateContext { cryptDevice cryptContext& cryptAlgo }
      DeviceOpen          { device& cryptUser^ deviceType name* }
      Encrypt             { cryptContext buffer= length# }
      End                 { }
      ExportCert          { certObject% maxlength! certObjectLength& \
                                certFormatType^ certificate }
      ExportKey           { encryptedKey% maxlength! encryptedKeyLength& \
                                exportKey sessionKeyContext }
      ExportKeyEx         { encryptedKey: maxlength! encryptedKeyLength& \
                                formatType exportKey sessionKeyContext }
      FlushData           { cryptHandle }
      GenerateKey         { cryptContext }
      GenerateKeyAsync    { cryptContext }
      GetAttribute        { cryptObject attributeType^ value& }
      GetAttributeString  { cryptObject attributeType^ value: valueLength& }
      GetCertExtension    { certificate oid* criticalFlag& extension: \
                                maxlength! extensionLength& }
      GetPrivateKey       { cryptHandle cryptContext& keyIDtype^ keyID* \
                                password* }
      GetPublicKey        { cryptObject publicKey& keyIDtype keyID= }
      ImportCert          { certObject= certObjectLength cryptUser^ \
                                certificate& }
      ImportKey           { encryptedKey= maxlength# importContext \
                                sessionKeyContext }
      KeysetClose         { keyset }
      KeysetOpen          { keyset& cryptUser^ keysetType^ name* options^ }
      PopData             { envelope buffer% length bytesCopied& }
      PushData            { envelope buffer= length# bytesCopied& }
      SetAttribute        { cryptObject^ attributeType^ value^ }
      SetAttributeString  { cryptObject^ attributeType^ value= valueLength# }
      SignCert            { certificate signContext }
    }

}
