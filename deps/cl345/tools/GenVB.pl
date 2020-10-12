#!/usr/bin/perl
# script to translate th cryptlib C interface into a Visual Basic interface module
# Copyright (C) 2003-2004 Wolfgang Gothier

#####
#       G E N V B . P L   $Id: GenVB.pl,v 1.3 2009/07/13 20:27:20 wogo Exp $
#       --------------------------------------------------------------------
#
#       PERL script for translation of the cryptlib header file
#            into a Visual Basic interface file for Cryptlib (CL32.DLL).
#
#            This script does the translation of C-statements into
#            Visual Basic code. (But only as much as is needed in 
#            cryptlib.h, -NOT- usable as general translation utility)
#
#       --------------------------------------------------------------------
#
#       SYNTAX:
#           perl GenVB.pl <cryptlib.h>
#
#               cryptlib.h ... (optional) Pathname of crytlib header file
#                              default is "cryptlib.h"
#
#               creates the Visual Basic interface file with same basic name 
#               and extension ".bas" in the same directory as the source file
#               default is "cryptlib.bas"
#
#####

use strict;
use warnings;

use File::stat;
use File::Basename;

my $FileName = shift @ARGV || 'cryptlib.h';		# default filename is "cryptlib.h"
my %DEFINED = ( 1, 1,                         # ifdef 1 is to be included
                "USE_VENDOR_ALGOS", 0 );			# set to 1 to include #IFDEF USE_VENDOR_ALGOS
my $Startline = qr{^#endif\s+\/\*\s+_CRYPTLIB_DEFINED\s+\*\/};	# ignore all lines before this one

my ($FileBase, $Path, $Ext) = fileparse($FileName, qr{\.[^.]*$});
die("\"usage: $0 cryptlib.h\"\nParameter must be a C header file\nStop") unless ($Ext =~ m/^\.h$/i) && -r $FileName;
my ($Infile, $Outfile) = ($Path.$FileBase.'.h', $Path.$FileBase.'.bas');
my $cryptlib_version;

open(INFILE, "<$Infile") or die "Open error on $Infile: $!";
open (OUTFILE, ">$Outfile") or die "Open error on $Outfile: $!";
print "Transforming \"$Infile\" into \"$Outfile\"\n";
my $Default = select(OUTFILE);


# Ignore all input lines before (and including) $Startline
while (<INFILE>) {
	$cryptlib_version = $_ if m{#define\s+CRYPTLIB_VERSION\s+};
	last if m/$Startline/;
}

# array to contain the preprocessed input lines:
my @source;

push @source, VBHeader($Infile);
push @source, $cryptlib_version if $cryptlib_version;

my $INACTIVE = 0;
my $LEVEL = 0;
my $COMMENT = 0;
# handle conditionals, include conditional code only if definition agrees with %DEFINED
while (<INFILE>) { 
    # remove preprocessor symbols
    s/C_CHECK_RETVAL//;
    s/C_NONNULL_ARG\s*\(\s*\([ \t0-9,]+\s*\)\s*\)//;

    # remove tabs
    1 while s/\t/' ' x (length($&)*4 - length($`)%4)/e;

    if (/^\s*#if(\s|def\s)(\w+)/) {
        $LEVEL += 1;
        $INACTIVE += 1 unless $DEFINED{$2};
        next;
    }
    if (/^\s*#if\s\(/) {		#if (anyexpression) assumed always false
        $LEVEL += 1;
        $INACTIVE += 1;
        next;
    }
    if (/^\s*#ifndef\s(\w+)/) {
        $LEVEL += 1;
        $INACTIVE += 1 if $DEFINED{$1};
        next;
    }
    if (/^\s*#(else|elif)\b/) {
        $INACTIVE = 1-$INACTIVE;
        next;
    }
    if (/^\s*\#endif\b/) {
        $LEVEL -= 1;
        $INACTIVE = 0;
        next;
    }
                                          # translate comments
    if (/\/\*(.*)\*\/\s*$/) {
        if ($1 !~ m(\*/)) {
            s!/\*(.*)\*/\s*$!'$1\n!
        }
    }

    if ($COMMENT) {
        $_ = "'".$_ unless s/^ /'/;
        $COMMENT = 0 if s/\*\/\s*$/\n/;
        s/\*\*$/***/;
    }
    $COMMENT = 1 if s/^(\s*)\/\*\*(.*)$/'**$1$2/;
    $COMMENT = 1 if s/^(\s*)\/\*(.*)$/'$1 $2/;

    push @source, $_ unless $INACTIVE;
}

# preprocessing finished, translation to VB code follows

my $Warn="";

while ($_ = shift @source) {
		# ignore special C++ handling
    if (/#ifdef\s+__cplusplus/) {
        $_ = shift @source  while (!(/#endif/));
        $_ = shift @source;
    }
    
    # continued lines
    if (s/\\$//) {
        $_ .= shift @source;
        redo if @source;
    }
    
    # incomplete typedef / enum lines
    if (/^\s*(typedef\s+enum|typedef\s+struct|enum)\s*\{[^}]*$/) {
        $_ .= shift @source;
        redo if @source;
    }
    
    # incomplete procedure calls
    if (/^\s*C_RET\s+\w+\s*\([^)]*$/) {
        $_ .= shift @source;
        redo if @source;
    }
		# lines are complete now, do the translation

    # hex values
    s{0x([0-9a-fA-F]+)}{&H$1}g;
    
		# constant definitions
		s/^\s*#define\s+(\w+)\s+(\w+|[+\-0-9]+|&H[0-9a-fA-F]+)/  Public Const $1 As Long = $2/;
		s/^\s*#define\s+(\w+)\s+\(\s*(\w+|[+\-0-9]+|&H[0-9a-fA-F]+)\s*\)/  Public Const $1 As Long = $2/;

    # typedef struct
    if (s!^(\s*)typedef\s+struct\s*{([^}]*)}\s*(\w+)\s*;!&typelist(split(/;/,$2))!e) {
        $_ = "$1Public Type $3 $_\n$1End Type\n";
    }

		# typedef enum ( with intermediate constant definitions )
    if (s!^\s*typedef\s+enum\s*{([^}]+=\s*\d+\b[^}]+)}\s*(\w+);!&enumt(split(/\n/,$1))!e) {
        $_ = "Public Enum $2\n$_"."End Enum\n";
    }

		# typedef enum
    if (s!^\s*typedef\s+enum\s*{([^}]+)}\s*(\w+);!&enumt(split(/\n/,$1))!e) {
        $_ = "Public Enum $2\n$_"."End Enum\n";
    }

		# "simple" typedef
    s/^\s*typedef\s+(\w+)\s+(\w+);/"REM  $2 = ".&typeconv($1)/e;       # ignore redefinitions

		# "simple" enum
    s!^\s*enum\s*{([^}]+)}\s*;!&enums(split(/,/,$1))!e;

		# translate function declarations without params
		s/\bC_RET\s*(\w+)\s*\(\s*void\s*\)\s*;/Public Declare Function $1 Lib \"CL32.DLL\" () As Long\n\n/;

		# translate function declarations with params
    if (s!^\s*C_RET\s*(\w+)\s*\(\s*([^)]+)\s*\)\s*;!&convpar(split(/,/,$2))!e) {
        chomp;
        $_ = "Public Declare Function $1 Lib \"CL32.DLL\" ($_) As Long\n\n";
        $_ = "' ***Warning: function '$1' $Warn\n$_" if ($Warn);
    }
		
		# C-macro definitions are ignored
    if (s/\s*#define\s+(.*)/$1/) {
        s/\n/\n'/g;
        s/\s+$//;
        $_ = "' C-macro not translated to Visual Basic code: \n'   #define $_\n";
    }

		# translation is done, output lines now
    print "$_" if @source;
}
print VBFooter();

select($Default);

exit;

# subroutine definitions follow:

sub VBHeader {
	my $Infile = shift;
	my $fstat = stat($Infile) if (-f $Infile && -r $Infile) or die "$Infile not readable";
	my $infile_size = $fstat->size;
	my $infile_time = localtime($fstat->mtime);
	my $filename = basename($Infile);
	my $now = (localtime())[5]+1900;
return <<ENDOFHEADER;
Attribute VB_Name = "CRYPTLIB"

Option Explicit

'*****************************************************************************
'*                                                                           *
'*                        cryptlib External API Interface                    *
'*                       Copyright Peter Gutmann 1997-$now                   *
'*                                                                           *
'*                 adapted for Visual Basic Version 6  by W. Gothier         *
'*****************************************************************************


'-----------------------------------------------------------------------------

'This file has been created automatically by a perl script from the file:
'
'"$filename" dated $infile_time, filesize = $infile_size.
'
'Please check twice that the file matches the version of $filename
'in your cryptlib source! If this is not the right version, try to download an
'update from "http://cryptlib.sogot.de/". If the filesize or file creation
'date do not match, then please do not complain about problems.
'
'Examples using Visual Basic are available on the same web address.
'
'Published by W. Gothier, 
'mailto: problems\@cryptlib.sogot.de if you find errors in this file.

'-----------------------------------------------------------------------------

ENDOFHEADER

}

sub VBFooter {
return <<ENDFOOTER;

'*****************************************************************************
'*                                                                           *
'*                    End of Visual Basic Functions                          *
'*                                                                           *
'****************************************************************************}

ENDFOOTER
}

# subroutine to handle simple enum elements
sub enums {
    my $Index = 0; # startvalue = 0 for enum entries
    my $_S;
    foreach (@_) {
        chomp;
        s/^\s+//;   # delete leading whitespace
        if (m/(\w+)\s*=\s*(\d+).*$/) {
        		# new value is being set, $index must be updated
            $_S .= "  Public Const $1 As Long = $2\n";
            eval($Index = $2+1);
        }
        else {
            $_S .= "  Public Const $_ As Long = ".$Index++."\n";
        }
    }
    return $_S;
}

# subroutine to handle typedef enum ( with intermediate constant definitions )
sub enumt {
    my $LINES = "";
    my $parval;
    my $newpar;
    foreach $parval (@_) {
        if ($parval =~ /^([^',]+)\,\s*([^'\t \n]+)/) {
            $parval =~ s/^(\s*)([^',]+)\,\s*(.*)$/$1$2/;
            $newpar = "$1$3";
        } 
        else { 
            $newpar = "" 
        }
        $parval =~ s/\,\s*$//;
        $parval =~ s/^(\s*\w+\s*\=\s*[^']+\s*)\,/$1 /g;
        $parval =~ s/^(\s*\w+\s*)\,/$1 /g;
        $LINES .= "$parval\n";
        $parval = $newpar;
        redo if $parval;
    }
    return $LINES;
}
#   handle the lines of a "typedef struct { ... } structname"
sub typelist {
    my $tmp = "";
    foreach my $par (@_) {
        while ($par =~ s/^(\s*)\'(.+)\n(.*)/$3/) {   # embedded comments
            $tmp .= "$1'$2\n";
        }
        if ($par =~ s/^(\s*)(.*)\s(\w+)\s*\[\s*(\w+)\s*\]\s*$//) {    # index conversion
            $tmp .= "$1$3($4-1) As ".&typeconv($2);
        }
        elsif ($par =~ s/^(\s*)(.*)\s(\w+)\s*$//) {  # normal conversion
            $tmp .= "$1$3 As ".&typeconv($2);
        }
        else {$tmp .= $par}                          # leave it alone
    }
    return $tmp;
}

#   type conversion from C types to Visual Basic types
#   only the types from cryptlib.h are handled !!!
sub typeconv {
    my $param = shift;
    return $param if $param =~ s/\bint\b/Long/;
    return $param if $param =~ s/\bunsigned char\b/Byte/;
    return $param if $param =~ s/\bchar\s+C_PTR\b/String/;
    return $param if $param =~ s/\bchar\b/Byte/;
    return $param if $param =~ s/\bC_CHR\b/Byte/;						  # new cryptlib type C_CHR in current V3.1
    return $param if $param =~ s/\bvoid\s+C_PTR\b/String/;
    return $param if $param =~ s/\bC_STR\b/String/;						# new cryptlib type C_STR in current V3.1

    return $param if $param =~ s/\bCRYPT_CERTIFICATE\b/Long/;
    return $param if $param =~ s/\bCRYPT_CONTEXT\b/Long/;
    return $param if $param =~ s/\bCRYPT_DEVICE\b/Long/;
    return $param if $param =~ s/\bCRYPT_ENVELOPE\b/Long/;
    return $param if $param =~ s/\bCRYPT_KEYSET\b/Long/;
    return $param if $param =~ s/\bCRYPT_SESSION\b/Long/;
    return $param if $param =~ s/\bCRYPT_USER\b/Long/;
    return $param if $param =~ s/\bCRYPT_HANDLE\b/Long/;
    return $param;
}

#   parameter conversion for parameter lists in procedure calls
sub convpar {
    my $tmp = '';
    $Warn = "";
    LOOP: foreach my $parval (@_) {
        $tmp .= &convpar1($parval).", _\n";
    }
    $tmp =~ s/, _\n$//;
    return $tmp;
}

#   conversion of a single parameter in a parameter list
sub convpar1 {
    my $par = shift;
    if ($par =~ s/^\s*(C_IN\s+|C_IN_OPT\s+)(.+)\s+(\w+)\s*/&typeconv($2)/e) {
        return " ByVal $3 As $par";
    }
    if ($par =~ s/^\s*C_INOUT\s+(.+)\s+(\w+)\s*/&typeconv($1)/e) {
        $Warn = "will replace the String '$2'";
        return " ByVal $2 As $par";
    }
    if ($par =~ s/^\s*(C_OUT\s+|C_OUT_OPT\s+)void\s+C_PTR\s+(\w+)\s*/$2/) {
        $Warn = "will modify the String '$par'";
        return " ByVal $par As String";
    }
    if ($par =~ s/^\s*(C_OUT\s+|C_OUT_OPT\s+)(.+)\s+C_PTR\s+(\w+)\s*/&typeconv($2)/e) {
        return " ByRef $3 As $par";
    }
    if ($par =~ s/^\s*(C_OUT\s+|C_OUT_OPT\s+)(.+)\s+(\w+)\s*/&typeconv($2)/e) {
        return " $3 As $par";
    }
    return $par;
}

