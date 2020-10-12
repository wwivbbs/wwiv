#!/usr/bin/perl
# script to translate th cryptlib C interface into a Delphi (Pascal) interface module
# Copyright (C) 2003-2004 Wolfgang Gothier

#####
#       G E N P A S . P L   $Id: GenPas.pl 39 2010-10-21 16:28:47Z wogo $
#       -----------------------------------------------------------------------
#
#       PERL script for translation of the cryptlib header file
#            into a Delphi (R) interface file for Cryptlib (CL32.DLL).
#
#            This script does the translation of C-statements into
#            Pascal (Delphi) code. (But only as much as is needed in 
#            cryptlib.h, -NOT- usable as general translation utility)
#
#       --------------------------------------------------------------------
#
#       SYNTAX:
#           perl GenPas.pl <cryptlib.h>
#
#               cryptlib.h ... (optional) Pathname of crytlib header file
#                              default is "cryptlib.h"
#
#               creates the Delphi interface file with same basic name 
#               and extension ".pas" in the same directory as the source file
#               default is "cryptlib.pas"
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
my ($Infile, $Outfile) = ($Path.$FileBase.'.h', $Path.$FileBase.'.pas');
my $cryptlib_version;

open(INFILE, "<$Infile") or die "Open error on $Infile: $!";
open (OUTFILE, ">$Outfile") or die "Open error on $Outfile: $!";
print "Transforming \"$Infile\" into \"$Outfile\"\n";
my $Default = select(OUTFILE);

# Ignore all input lines before (and including) $Startline (except version def)
while (<INFILE>) {
	$cryptlib_version = $_ if m{#define\s+CRYPTLIB_VERSION\s+};
	last if m/$Startline/;
}

# array to contain the preprocessed input lines:
my @source;

push @source, PascalHeader($Infile);
push @source, $cryptlib_version if $cryptlib_version;

my $INACTIVE = 0;
my $LEVEL = 0;
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

    push @source, $_ unless $INACTIVE;
}

# preprocessing finished, translation to pascal code follows

my $const="\nconst\n";
my $type="\ntype\n";

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

		# constant definitions
    if (s/^\s*#define\s+(\w+)\s+(\w+|[+\-0-9]+)/$const  $1 = $2;/) {
        $const="";
        $type="\ntype\n";
    }
    
		# constant definitions with parenthesis
    if (s/^\s*#define\s+(\w+)\s+\(\s*(\w+|[+\-0-9]+)\s*\)/$const  $1 = $2;/) {
        $const="";
        $type="\ntype\n";
    }
    
    # hex values
    s{0x([0-9a-fA-F]+)}{\$$1}g;
    
    # typedef struct
    if (s!^(\s*)typedef\s+struct\s*{([^}]*)}\s*(\w+)\s*;!&typelist(split(/;/,$2))!e) {
        $_ = "$1$type  $3 = record  $_\n  end;\n";
        $type="";
        $const="\nconst\n"
    }

		# typedef enum ( with intermediate constant definitions )
    if (s!^\s*typedef\s+enum\s*{([^}]+=\s*\d+\b[^}]+)}\s*(\w+);!&enumt(split(/\n/,$1))!e) {
        $_ = "$type $2 = Integer;\nconst\n$_\n";
        $const="";
        $type="\ntype\n";
    }

		# typedef enum
    if (s/^\s*typedef\s+enum\s*{([^}]+)}\s*(\w+);/$type  $2 = (  $1\n  );/) {
    		my $Typeis = $2;
        my %redefs;
        # check, if there are embedded redefinitions
        while (s!\s*(\w+)\s*=\s*([_a-zA-Z]\w+),!!g) { 
        		$redefs{$1} = $2 if $1;
        };
        $type="";
        $const="\nconst\n";
        if (%redefs) {
        		# handle embedded redefinitions after type definition
        		$_ .= $const;
        		$const="";
        		$type="\ntype\n";
        		for my $redef (keys %redefs) {
        				$_ .= "  $redef: $Typeis = $redefs{$redef};\n";
        		}
        }
    }

		# "simple" typedef
    if (s/^\s*typedef\s+(\w+)\s+(\w+);/"$type  $2 = ".&typeconv($1).";"/e) {
        $type="";
        $const="\nconst\n"
    }

		# "simple" enum
    if (s!^\s*enum\s*{([^}]+)}\s*;!&enums(split(/,/,$1))!e) {
        $_ = "\n$const$_\n";
        $const="";
        $type="\ntype\n";
    }

		
    s/(\s*)#ifdef\s+(\w+)/$1\{\$IFDEF $2\}/g;
    s/(\s*)#if\s+0\b/$1\{\$IFDEF false\}/g;
    s/(\s*)#if\s+1\b/$1\{\$IFDEF true\}/g;
    s/(\s*)#endif\s*/$1\{\$ENDIF\}\t/g;
    
    # translate comments
    s/\/\*\*/{**/g;
    s/\/\* /{  /g;
    s/\*\*\//**}/g;
    s/\ \*\//  }/g;
    s/\*\//}/g;

		# functions without parameters
    s/\bC_RET\s*(\w+)\s*\(\s*void\s*\)\s*;/function $1: Integer;\n{\$IFDEF WIN32} stdcall; {\$ELSE} cdecl; {\$ENDIF} external cryptlibname;\n\n/;

		# function declarations with parameters
    if (s/^\s*C_RET\s*(\w+)\s*\(\s*([^\)]+)\s*\)\s*;/&convpar(split(\/\,\/,$2))/e) {
        chomp($_);
        $_ = "function $1( $_ ): Integer;\n{\$IFDEF WIN32} stdcall; {\$ELSE} cdecl; {\$ENDIF} external cryptlibname;\n\n";
    }

		# C-macro definitions are ignored
    if (s/\s*#define\s+(.*)/$1/) {
        s/\{/</g;
        s/\}/>/g;
        s/\s+$//;
        $_ = "{ C-macro not translated to Delphi code: \n{   #define $_ }\n";
    }

		# translation is done, output lines now
    print "$_" if @source;
}
print PascalFooter();

select($Default);

exit;

# subroutine definitions follow:

sub PascalHeader {
	my $Infile = shift;
	my $fstat = stat($Infile) if (-f $Infile && -r $Infile) or die "$Infile not readable";
	my $infile_size = $fstat->size;
	my $infile_time = localtime($fstat->mtime);
	my $filename = basename($Infile);
	my $now = (localtime())[5]+1900;
return <<ENDOFHEADER;
unit cryptlib;

interface

{****************************************************************************
*                                                                           *
*                     Cryptlib external API interface                       *
*                    Copyright Peter Gutmann 1997-$now                      *
*                                                                           *
*        adapted for Delphi Version 5 (32 bit) and Kylix Version 3          *
*                              by W. Gothier                                *
****************************************************************************}


{------------------------------------------------------------------------------

 This file has been created automatically by a perl script from the file:

 "$filename" dated $infile_time, filesize = $infile_size.

 Please check twice that the file matches the version of $filename
 in your cryptlib source! If this is not the right version, try to download an
 update from "http://cryptlib.sogot.de/". If the filesize or file creation
 date do not match, then please do not complain about problems.

 Published by W. Gothier, 
 mailto: problems\@cryptlib.sogot.de if you find errors in this file.

-------------------------------------------------------------------------------}

{\$A+}  {Set Alignment on}
{\$F+}  {Force function calls to FAR}
{\$Z+}  {Force all enumeration values to Integer size}

const
  {\$IFDEF WIN32}
    cryptlibname = 'CL32.DLL';  { dynamic linkname for Windows (Delphi) }
  {\$ELSE}
    cryptlibname = 'libcl.so';  { library name for Unix/Linux  (Kylix) }
                 { symbolic link should be used for libcl.so -> libcl.so.3.x.y }
  {\$ENDIF}

ENDOFHEADER

}

sub PascalFooter {
return <<ENDFOOTER;


implementation

{ no implementation code now }

end.
ENDFOOTER
}

# subroutine to handle simple enum elements
sub enums {
    my $Index = 0; # startvalue = 0 for enum entries
    my $_S;
    foreach (@_) {
        chomp;
        s/^\s+//;   # delete leading whitespace
        s!/\*!{!g; # translate comment brackets
        s!\*/!}!g;
        if (m/(\{[^\}]*\})?\s*\w+\s*=\s*(\d+).*$/) {
        		# new value is being set, $index must be updated
            $_S .= "  $_;\n";
            eval($Index = $2+1);
        }
        else {
            $_S .= "  $_ = ".$Index++.";\n";
        }
    }
    return $_S;
}

# subroutine to handle typedef enum ( with intermediate constant definitions )
sub enumt {
    my $INDEX = 0;
    my $LINES = "";
    my %VALS;
    my $tmp;
    my $comment;
    LOOP: foreach (@_) {
        chomp;
        s/^\s+//;
        s/\s+$//;
        s/\/\*/{/g;
        s/\*\//}/g;
        # multiline comments
        if ($comment or m!{[^}]*$!) {
        		$LINES .= "  $_\n";
        		$comment = not m!}!;
        		next LOOP if $comment;
        		s!.*}!!;
        		next unless $_ eq "\n";
        }
        # comment only line
        if (m!{[^}]}$!) {
        		$LINES .= "  $_\n";
        		next LOOP;
        }
        # enumval = enumval +/- number
        elsif (m/^(\w+)\s*=\s*(\w+)\s*([-+])\s*(\d+),?(.*)$/) {
            eval( $tmp = eval("$VALS{$2} $3 $4") );
            $LINES .= "  $1 = $tmp;";
            $VALS{$1} = $tmp;
            eval($INDEX = $tmp+1);
            $_ = $5;
            redo LOOP;
        }
        # enumval = number
        elsif (m/^(\w+)\s*=\s*(\d+),?(.*)$/) {
            $LINES .= "  $1 = $2;";
            $VALS{$1} = $2;
            eval($INDEX = $2+1);
            $_ = $3;
            redo LOOP;
        }
        # enumval = enumval
        elsif (m/^\s*(\w+)\s*=\s*(\w+),?(.*)$/) {
            $tmp = $VALS{$2};
            $LINES .= "  $1 = $tmp; { = $2 }";
            $VALS{$1} = $tmp;
            $_ = $3;
            redo LOOP;
        }
        # enumval,
        elsif (m/^\s*(\w+)\,?(.*)$/) {
            $LINES .= "  $1 = $INDEX;";
            $VALS{$1} = $INDEX;
            $INDEX += 1;
            $_ = $2;
            redo LOOP;
        }
        else { $LINES .= "  $_\n" }
    }
    return $LINES;
}

# subroutine to translate struct elements into record elements
sub typelist {
    my $tmp = "";
    foreach  (@_) {
    		# handle comment at start of splitted line
        while ($_ =~ s!^(\s*)/\*(.+)\*/!!s) {
            $tmp .= $1.'{'.$2.'}';
        }
        # translate fields into arrays 
        if (s!^(\s*)(.*)\s+(\w+)\s*\[\s*(\w+)\s*\]\s*$!!) {
            $tmp .= "$1$3: array[0 .. $4-1] of ".&typeconv($2).";";
        }
        # translate normal elements
        elsif (s!^(\s*)(\w+)\s+(\w+)\s*$!!) {
            $tmp .= "$1$3: ".&typeconv($2).";";
        }
        # copy line, if nothing matched
        else {
			$tmp .= $_;
			$tmp .= ";" unless $_;
		}
    }
    return $tmp;
}

# subroutine transforms some C types to Delphi types
sub typeconv {
    my $param = shift;
    return $param if $param =~ s/\bint\b/Integer/;					#	int    				-> Integer
    return $param if $param =~ s/\bunsigned char\b/byte/;		# unsigned char	-> byte
    return $param if $param =~ s/\bvoid\s+C_PTR\b/Pointer/;	# void C_PTR		-> Pointer
    return $param if $param =~ s/\bvoid\b/Pointer/;					# void					-> Pointer
    return $param if $param =~ s/\bchar\s+C_PTR\b/PAnsiChar/;		# char C_PTR		-> PAnsiChar
    return $param if $param =~ s/\bC_STR\b/PAnsiChar/;					# char C_PTR		-> PAnsiChar
    return $param if $param =~ s/\bC_CHR\b/char/;						# char C_PTR		-> PAnsiChar
    return $param;
}

# subroutine to convert a list or function parameters
sub convpar {
    my @tmp;
    LOOP: 
    foreach (@_) {
        push( @tmp, &convpar1($_) );
    }
    return join(";\n  ", @tmp);
}

# subroutine to translate C params to Delphi params
sub convpar1 {
    my $par = shift;
    return "const $3: $par" if ($par =~ s/^\s*(C_IN\s+|C_IN_OPT\s+)(.+)\s+(\w+)\s*/&typeconv($2)/e);
    return "$2: $par"       if ($par =~ s/^\s*C_INOUT\s+(.+)\s+(\w+)\s*/&typeconv($1)/e);
    return "$par"           if ($par =~ s/^\s*(C_OUT\s+|C_OUT_OPT\s+)void\s+C_PTR\s+(\w+)\s*/$2: Pointer/);
    return "var $3: $par"   if ($par =~ s/^\s*(C_OUT\s+|C_OUT_OPT\s+)(.+)\s+(?:C_PTR)?\s+(\w+)\s*/&typeconv($2)/e);
    return $par;
}
