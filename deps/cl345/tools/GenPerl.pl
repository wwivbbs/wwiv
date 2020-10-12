#!/usr/bin/perl
# script to translate th cryptlib C interface into a Perl header interface module
# Copyright (C) 2007 Alvaro Livraghi

#####
#       G E N P E R L . P L   Version 0.2 (last changes 2008-08-17)
#       --------------------------------------------------------------------
#       Based upon GenVB.pl by Wolfgang Gothier
#
#       PERL script for translation of the cryptlib header file
#            into a Perl header file used by Perl interface package 
#            for Cryptlib (PerlCryptLib.pm).
#
#            This script does the translation of C-statements into
#            Perl code. (But only as much as is needed in 
#            cryptlib.h, -NOT- usable as general translation utility)
#
#       --------------------------------------------------------------------
#
#       SYNTAX:
#           perl GenPerl.pl <cryptlib.h> <PerlCryptLib.ph>
#
#               cryptlib.h ........ (optional) Pathname of crytlib header file
#                                              default is "cryptlib.h"
#               PerlCryptLib.ph ... (optional) Pathname of PerlCrytLib header file
#                                              default is "PerlCryptLib.ph"
#
#               creates the Perl interface file with same basic name 
#               and extension ".ph" in the same directory as the source file
#               default is "PerlCryptLib.ph"
#
#####

use strict;
use warnings;

use File::stat;
use File::Basename;
use Data::Dumper;
#use Tie::IxHash;

my $C = "C\t";
my $PERL = "PERL\t";

my $DEBUG = grep /^--debug$/, @ARGV;			# print debug info on STDERR
my $inFileName  = shift @ARGV || 'cryptlib.h';	# default filename is "cryptlib.h"
my %DEFINED = ( 1, 1,                     		# ifdef 1 is to be included
                "USE_VENDOR_ALGOS", 0 );		# set to 1 to include #IFDEF USE_VENDOR_ALGOS
my $Startline = qr{^#define C_INOUT};			# ignore all lines before this one

my ($inFileBase, $inPath, $inExt) = fileparse($inFileName, qr{\.[^.]*$});
die("\"usage: $0 cryptlib.h\"\nParameter must be a C header file\nStop") unless ($inExt =~ m/^\.h$/i) && -r $inFileName;

my $outFileName = shift @ARGV || $inPath.'PerlCryptLib.ph';		# default filename is "PerlCryptLib.ph"
my ($outFileBase, $outPath, $outExt) = fileparse($outFileName, qr{\.[^.]*$});

my ($Infile, $Outfile) = ($inPath.$inFileBase.'.h', $outPath.$outFileBase.$outExt);
my $cryptlib_version;

open(INFILE, "<$Infile") or die "Open error on $Infile: $!";
open (OUTFILE, ">$Outfile") or die "Open error on $Outfile: $!";
print "Transforming \"$Infile\" into \"$Outfile\"\n";
my $Default = select(OUTFILE);

print STDERR qq[
${C}#include <stdio.h>
${C}#include <stdlib.h>
${C}#include "$Infile"
${C}int main(void) {
] if $DEBUG;

print STDERR qq[
${PERL}#!/usr/bin/perl -W
${PERL}use strict;
${PERL}use warnings;
${PERL}require "$Outfile";
] if $DEBUG;



# Ignore all input lines before (and including) $Startline
while (<INFILE>) {
	$cryptlib_version = $_ if m{#define\s+CRYPTLIB_VERSION\s+};
	last if m/$Startline/;
}

# array to contain the preprocessed input lines:
my @source;

push @source, PERLHeader($Infile);
push @source, $cryptlib_version if $cryptlib_version;

my $INACTIVE = 0;
my $LEVEL = 0;
my $COMMENT = 0;
# handle conditionals, include conditional code only if definition agrees with %DEFINED
while (<INFILE>) { 

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
            s!/\*(.*)\*/\s*$!#$1\n!
        }
    }

    if ($COMMENT) {
        $_ = "#".$_ unless s/^ /#/;
        $COMMENT = 0 if s/\*\/\s*$/\n/;
        s/\*\*$/***/;
    }
    $COMMENT = 1 if s/^(\s*)\/\*\*(.*)$/#**$1$2/;
    $COMMENT = 1 if s/^(\s*)\/\*(.*)$/#$1 $2/;

    push @source, $_ unless $INACTIVE;
}

# preprocessing finished, translation to PERL code follows

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
    
    # continued function declaration
    if (s/\,s*?$/,/) {
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
    #s{0x([0-9a-fA-F]+)}{&H$1}g;
    
	# constant definitions
	#s/^\s*#define\s+(\w+)\s+(\w+|[+\-0-9]+|&H[0-9a-fA-F]+)/  Public Const $1 As Long = $2/;
	#s/^\s*#define\s+(\w+)\s+(\w+|[+\-0-9]+|&H[0-9a-fA-F]+)/\tsub $1 { $2 }/;
	s/^\s*#define\s+(\w+)\s+\(?\s*(\w+|[+\-0-9]+|&H[0-9a-fA-F]+)\s*\)?\s*/\tsub $1 { $2 }\n/;

    # typedef struct
    if (s!^(\s*)typedef\s+struct\s*{([^}]*)}\s*(\w+)\s*;!&typelist(split(/;/,$2))!e) {
        $_ = "sub $3\n{\n\t{\n$_\t}\n}\n";
    }

	# typedef enum ( with intermediate constant definitions )
    if (s!^\s*typedef\s+enum\s*{([^}]+=\s*\d+\b[^}]+)}\s*(\w+);!&enumt(split(/\n/,$1))!e) {
        $_ = "##### BEGIN ENUM $2 $_##### END ENUM $2\n";
    }

	# typedef enum
    if (s!^\s*typedef\s+enum\s*{([^}]+)}\s*(\w+);!&enumt(split(/\n/,$1))!e) {
        $_ = "##### BEGIN ENUM $2\n$_##### END ENUM $2\n";
    }

	# "simple" typedef
    s/^\s*typedef\s+(\w+)\s+(\w+);/sub $2 { 0 }/;

	# "simple" enum
    s!^\s*enum\s*{([^}]+)}\s*;!&enums(split(/,/,$1))!e;

	# translate function declarations without params
	if ( s/(\bC_RET\s*\w+\s*\(\s*[^)]+\s*\)\s*;)/#$1/ ) {
		s/\n/\n#/g;
	}
	
	if ( s/^(\s*?)(C_CHECK_RETVAL|C_NONNULL_ARG)(.*?)/# $1$2$3/ ) {
		s/\n/\n#/g;
	}

	# C-macro definitions are ignored
    if (s/\s*#define\s+(.*)/$1/) {
        s/\n/\n#/g;
        s/\s+$//;
        $_ = "# C-macro not translated to Perl code but implemented apart: \n#   #define $_\n";
    }

	# translation is done, output lines now
    print "$_" if @source;
}
print PERLFooter();

print STDERR qq[
${C}return 0;
${C}}
] if $DEBUG;

print STDERR qq[
${PERL}exit(0);
] if $DEBUG;

select($Default);

exit 0;

# subroutine definitions follow:

sub PERLHeader {
	my $Infile = shift;
	my $fstat = stat($Infile) if (-f $Infile && -r $Infile) or die "$Infile not readable";
	my $infile_size = $fstat->size;
	my $infile_time = localtime($fstat->mtime);
	my $filename = basename($Infile);
	my $now = (localtime())[5]+1900;
return <<ENDOFHEADER;

# *****************************************************************************
# *                                                                           *
# *                        cryptlib External API Interface                    *
# *                       Copyright Peter Gutmann 1997-$now                   *
# *                                                                           *
# *                 adapted for Perl Version 5.x  by Alvaro Livraghi          *
# *****************************************************************************
#
#
# ----------------------------------------------------------------------------
#
# This file has been created automatically by a perl script from the file:
#
# "$filename" dated $infile_time, filesize = $infile_size.
#
# Please check twice that the file matches the version of $filename
# in your cryptlib source! If this is not the right version, try to download an
# update from CPAN web site. If the filesize or file creation date do not match,
# then please do not complain about problems.
#
# Published by Alvaro Livraghi, 
# mailto: perlcryptlib\@gmail.com if you find errors in this file.
#
# -----------------------------------------------------------------------------
#

ENDOFHEADER

}

sub PERLFooter {
return <<ENDFOOTER;

#
# *****************************************************************************
# *                                                                           *
# *                    End of Perl Functions                                  *
# *                                                                           *
# *****************************************************************************
#

1; ##### End-of perl header file!

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
            $_S .= "  sub $1 { $2 }\n";
			print STDERR qq{${C}printf("$1: \%d\\n", $1);\n} if $DEBUG;
			print STDERR qq{${PERL}print "$1: ", \&$1(), "\\n";\n} if $DEBUG;
            eval($Index = $2+1);
        }
        else {
            $_S .= "  sub $_ { ".$Index++." }\n";
			print STDERR qq{${C}printf("$_: \%d\\n", $_);\n} if $DEBUG;
			print STDERR qq{${PERL}print "$_: ", \&$_(), "\\n";\n} if $DEBUG;
        }
    }
    return $_S;
}

# subroutine to handle typedef enum ( with intermediate constant definitions )
sub enumt {
    my $LINES = "";
    my $parval;
	my $lastValue = 0;
	#tie my %values, 'Tie::IxHash', ();
	my %values = ();
	my @lines = @_;
    foreach my $parval1 (@lines) {
    	#my ($val, $rem, $name, $value);
		my ($val1, $rem) = split('#', $parval1, 2);
		$rem = '' unless $rem;
		$rem =~ s/^\s*(.*?)\s*$/$1/;
		$LINES .= ($rem ? "\t# $rem" : '') . "\n";
		$val1 = '' unless $val1;
		$val1 =~ s/^\s*(.*?)\s*$/$1/;
		next unless $val1;
    	foreach $parval (split(',',$val1)) {
    		last unless defined($parval);
	    	my ($val, $name, $value);
			($val = $parval) =~ s/^\s*(.*?)\s*$/$1/;
			#$val = '' unless $val;
			#$val =~ s/^\s*(.*?)\s*$/$1/;
			if ( $val ne '' ) {
				($name, $value) = split('=', $val, 2);
				$name = '' unless $name;
				$name =~ s/^\s*(.*?)[\s\,]*$/$1/;
				$value = '' unless $value;
				$value =~ s/^\s*(.*?)[\s\,]*$/$1/;
				if ( $value eq ''  ||  $value =~ /^\d/ ) {
					$value = $lastValue unless $value;
					#$lastValue = $value + 1;
				} else {
					#$rem .= ' ==> ' . $value;
					#$lastValue = 
					$value = eval( join(' ', map { exists($values{$_}) ? $values{$_} : $_ } split(/\s+/,$value)) );
				}
				$lastValue = $value + 1;
			}
			if ( $name ) {
				#$lastValue = $value;
				foreach my $curname (split(',', $name)) {
					$curname =~ s/^\s*(.*?)\s*$/$1/;
					$values{$curname} = $value;
			        #$LINES .= ($curname ? "\tsub $curname { $value }" : '') . ($rem ? "\t# $rem" : '') . "\n";
			        $LINES .= ($curname ? "\tsub $curname { $value }" : '') . "\n";
					#++$lastValue;
					#print STDERR "$curname = $value\n";
					print STDERR qq{${C}printf("$curname: \%d\\n", $curname);\n} if $DEBUG;
					print STDERR qq{${PERL}print "$curname: ", \&${curname}(), "\\n";\n} if $DEBUG;
				}
			} else {
		        #$LINES .= ($rem ? "\t# $rem" : '') . "\n";
			}
    	}
    }
    #print STDERR Dumper(\%values);
    return $LINES;
}
#   handle the lines of a "typedef struct { ... } structname"
sub typelist {
    my $tmp = "";
	my $first = 0;
    foreach my $par (@_) {
        while ($par =~ s/^(\s*)\#(.+)\n(.*)/$3/) {   # embedded comments
            $tmp .= "\t# $2\n";
        }
        if ($par =~ s/^(\s*)(.*)\s(\w+)\s*\[\s*(\w+)\s*\]\s*$//) {    # index conversion
            $tmp .= $1 . (!$first++ ? ' ' : ',') . "$3 => ' ' x $4";
        }
        elsif ($par =~ s/^(\s*)(.*)\s(\w+)\s*$//) {  # normal conversion
            $tmp .= $1 . (!$first++ ? ' ' : ',') . "$3 => 0";
        }
        else {$tmp .= $par}                          # leave it alone
    }
    return $tmp;
}


