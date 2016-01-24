use 5.008;
use ExtUtils::MakeMaker;
use Data::Dumper;
use File::Find;

my @search_in = ( '/usr/local/include', '/usr/lib/gcc-lib', '/usr/include' );
push @search_in, $ENV{HOME} if defined( $ENV{HOME} );

my $CRYPT_LIB_HEADER      = 'cryptlib.h';
my $PERL_CRYPT_LIB_HEADER = $ENV{PERL_CRYPT_LIB_HEADER};

unless ( defined $PERL_CRYPT_LIB_HEADER ) {
	print "Looking for '$CRYPT_LIB_HEADER', in:\n";
	print "\t- $_\n" foreach @search_in;
	print "Please wait... ";
	my @found =
	  qx{find @search_in -type f -iname '$CRYPT_LIB_HEADER' 2>/dev/null};
	if ( scalar(@found) == 0 ) {
		print "NOT FOUND",   "\n";
		print "$0 ABORTED!", "\n";
		print "\n";
		print
"You need CryptLib source code distribution in order to build PerlCryptLib.",
		  "\n";
		exit 1;
	}
	print "DONE",       "\n";
	print "Found(ed):", "\n";
	my %found  = ();
	my $recent = 0;
	foreach my $h (@found) {
		chomp $h;
		print "\t", $h;
		my $v = qx{grep 'CRYPTLIB_VERSION' $h 2>/dev/null};
		$v =~ m/(\d+)/;
		$found{$1} = $h;
		$recent = int($1) if int($1) gt $recent;
		print "\t", "(Ver. ", $1, ")";
		print "\n";
	}

#	if ( scalar(@found) > 1 ) {
#		print "$0 ABORTED!", "\n";
#		print "\n";
#		print "You have to set environment variable PERL_CRYPT_LIB_HEADER to specify which one of the headers founded is to be used.", "\n";
#		print "\n";
#		print "ie:", "\n";
#		print "\n";
#		print "export PERL_CRYPT_LIB_HEADER='$found[0]'", "\n";
#		print "\n";
#		exit 1;
#	}
#	$PERL_CRYPT_LIB_HEADER = $found[0];
	$PERL_CRYPT_LIB_HEADER = $found{$recent};
	print "Using version '$recent' from '$found{$recent}'\n";
}

print "Writing 'PerlCryptLib.ph', please wait... ";
print
  qx{perl ./GenPerl.pl $PERL_CRYPT_LIB_HEADER ./PerlCryptLib.ph 1>/dev/null};
if ( $? != 0 ) {
	print "ERROR $?", "\n";
	exit 1;
}
print "OK", "\n";

WriteMakefile(
	'NAME'         => 'PerlCryptLib',
	'DISTNAME'     => 'PerlCryptLib',
	'VERSION_FROM' => 'PerlCryptLib.pm',
	'PREREQ_PM'    => {},
	'PM'           => {
		'PerlCryptLib.pm' => '$(INST_LIBDIR)/PerlCryptLib.pm',
		'PerlCryptLib.ph' => '$(INST_LIBDIR)/PerlCryptLib.ph'
	},
	(
		$] >= 5.005
		? (
			ABSTRACT =>
			  'PerlCryptLib - Perl interface to Peter Guttman\'s cryptlib API',
			AUTHOR => 'Alvaro Livraghi <perlcryptlib@gmail.com>'
		  )
		: ()
	),
	,
	'LIBS' => [
		join( " ", map { "-L$_" } split ":", $ENV{LD_LIBRARY_PATH} )
		  . ' -lresolv -lpthread -lcl'
	],
	'DEFINE'    => "-DCRYPTLIB_H=\\\"$PERL_CRYPT_LIB_HEADER\\\"",
	'INC'       => '-I.',
	'clean'     => { FILE => '*.ph .*_h __debug*' },
	'realclean' => { FILES => '*.ph .*_h __debug* *.inc' }
);

if ( eval { require ExtUtils::Constant; 1 } ) {

	my @names = ();
	ExtUtils::Constant::WriteConstants(
		NAME         => 'PerlCryptLib',
		NAMES        => \@names,
		DEFAULT_TYPE => 'IV',
		C_FILE       => 'const-c.inc',
		XS_FILE      => 'const-xs.inc'
	);
	print join( "\n", @names ), "\n";

}
else {

	use File::Copy;
	use File::Spec;
	foreach my $file ( 'const-c.inc', 'const-xs.inc' ) {
		my $fallback = File::Spec->catfile( 'fallback', $file );
		copy( $fallback, $file ) or die "Can't copy $fallback to $file: $!";
	}

}
