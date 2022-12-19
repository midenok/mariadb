package My::File::Config;

# ----------------------------------------------------------------------
# My::File::Config - Parse and utilize MariaDB's /etc/my.cnf and ~/.my.cnf files
#
# Based on MySQL::Config 1.04
#
# Copyright (c) 2003 Darren Chamberlain <darren@cpan.org>
# Copyright (c) 2022 MariaDB Corporation
# 
# ----------------------------------------------------------------------

use strict;
use base qw(Exporter);
use vars qw($VERSION $GLOBAL_CNF @EXPORT @EXPORT_OK);

use Carp qw(carp);
use File::Spec;

$VERSION    = '1.04-mariadb0';
$GLOBAL_CNF = "/etc/%s.cnf" unless defined $GLOBAL_CNF;
@EXPORT     = qw(load_defaults);
@EXPORT_OK  = qw(parse_defaults);

# ======================================================================
#                        --- Public Functions ---
# ======================================================================

sub load_defaults {
    my ($conf_file, $user_cnf, $groups, $argc, $argv);
    if (! ref $_[0]) {
        ($conf_file, $groups, $argc, $argv) = @_;
    } elsif (ref $_[0] eq 'HASH') {
        $conf_file= $_[0]->{conf_name};
        $user_cnf= $_[0]->{conf_file};
        $groups= $_[0]->{groups};
        $argc= $_[0]->{argc};
        $argv= $_[0]->{argv};
    }

    my ($ini, $field, @argv);
    $ini = [ ];

    # ------------------------------------------------------------------
    # Sanity checking:
    #   * $conf_file should be a string, defaults to "my"
    #   * $groups should be a ref to an array
    #   * $argc should be a ref to a scalar
    #   * $argv should be a ref to an array
    # ------------------------------------------------------------------
    $conf_file = "my" unless defined $conf_file && ! ref($conf_file);
    if (!defined $user_cnf) {
        $user_cnf = File::Spec->catfile($ENV{HOME}, ".${conf_file}.cnf");
    }

    $groups = [ $groups ]
        unless ref $groups eq 'ARRAY';

    if (defined $argc) {
        $argc = \$argc
            unless ref $argc eq 'SCALAR';
    }
    else {
        $argc = \(my $i = 0);
    }

    $argv = \@ARGV unless defined $argv;
    $argv = [ $argv ]
        unless ref $argv eq 'ARRAY';

    # ------------------------------------------------------------------
    # Parse the global config and user's config
    # ------------------------------------------------------------------
    _parse($ini, sprintf $GLOBAL_CNF, $conf_file);
    _parse($ini, $user_cnf);

    # ------------------------------------------------------------------
    # Pull out the appropriate pieces, based on @$groups
    # ------------------------------------------------------------------
    @$groups = map { $_->[0] } @$ini unless @$groups;
    $groups = join '|', map { quotemeta($_) } @$groups;

    push @argv, map { $$argc++; sprintf "--%s=%s", $_->[1], $_->[2] }
                 grep { $_->[0] =~ /^$groups$/ } @$ini;
    @$argv = (@argv, @$argv);

    1;
}

# ======================================================================
#                        --- Private Functions ---
# ======================================================================

# ----------------------------------------------------------------------
# _parse($file)
#
# Parses an ini-style file into an array of [ group, name, value ]
# array refs.
# ----------------------------------------------------------------------
sub _parse {
    my $ini = shift;
    my $file = shift;
    my $current;
    local ($_, *INI);

    return { } unless -f $file && -r _;

    $ini ||= [ ];
    unless (open INI, $file) {
        carp "Couldn't open $file: $!";
        return { };
    }
    while (<INI>) {
        s/[;#].*$//;
        s/^\s*//;
        s/\s*$//;

        next unless length;

        /^\s*\[(.*)\]\s*$/
            and $current = $1, next;

        $_ = join '=', $_, 1
            unless /=/;

        my ($n, $v) = split /\s*=\s*/, $_, 2;
        if ($v =~ /\s/) {
            $v =~ s/"/\\"/g;
            $v = qq("$v") 
        }

        push @$ini, [ $current, $n, $v ];
    }

    unless (close INI) {
        carp "Couldn't close $file: $!";
    }

    return $ini;
}

1;

__END__

=head1 NAME

My::File::Config - Parse and utilize MariaDB's /etc/my.cnf and ~/.my.cnf files

=head1 SYNOPSIS

    use MySQL::Config;

    my @groups = qw(client myclient);
    my $argc = 0;
    my @argv = ();

    load_defaults "my", \@groups, \$argc, \@argv;

=head1 DESCRIPTION

C<My::File::Config> emulates the C<load_defaults> function from
F<libmysqlclient>.  Just like C<load_defaults>, it will fill an aray
with long options, ready to be parsed by C<getopt_long>, a.k.a.
C<Getopt::Long>.

=head1 THE my.cnf FILE

MySQL's F<my.cnf> file is a mechanism for storing and reusing command
line arguments.  These command line arguments are grouped into
I<groups> using a simple INI-style format:

    ; file: ~/.my.cnf

    [client]
    user = darren
    host = db1
    pager = less -SignMEX

    [mytop]
    color = 1
    header = 0

Each element in C<[>, C<]> pairs is a I<group>, and each call to
C<load_defaults> will specify 0 or more groups from which to grab
options.  For example, grabbing the I<client> group from the above
config file would return the I<user>, I<host>, and I<pager> items.
These items will be formatted as command line options, e.g.,
I<--user=darren>.

=head1 USING My::File::Config

=head2 load_defaults("name", \@groups, \$count, \@ary)

C<load_defaults> takes 4 arguments: a string denoting the name of the
config file (which should generally be I<my>); a reference to an array
of groups from which options should be returned; a reference to a
scalar that will hold the total number of parsed elements; and a
reference to an array that will hold the final versions of the
extracted name, value pairs.  This final array will be in a format
suitable for processing with C<Getopt::Long>:

    --user=username
    --password=password

and so on.

If the final array reference is missing, C<@ARGV> will be used.  Options
will be pushed onto the end of the array, leaving what is already in
place undisturbed.

The scalar (the third argument to C<load_defaults>) will contain the
number of elements parsed from the config files.

=head1 USING SOMETHING OTHER THAN "my" AS THE FIRST STRING

This string controls the name of the configuration file; the names
work out to, basically F<~/.${cfg_name}.cnf> and
F</etc/${cnf_name}.cnf>.

If you are using this module for mysql clients, then this should
probably remain I<my>.  Otherwise, you are free to mangle this however
you choose:

    $ini = parse_defaults 'superapp', [ 'foo' ];

=head1 SUPPORT

C<My::File::Config> is supported by the author.

=head1 VERSION

This is C<My::File::Config>, version 1.04.

=head1 AUTHOR

Darren Chamberlain E<lt>darren@cpan.orgE<gt>

=head1 COPYRIGHT

(c) 2003 Darren Chamberlain
(c) 2022 MariaDB Corporation

This library is free software; you may distribute it and/or modify it
under the same terms as Perl itself.

=head1 SEE ALSO

L<Perl>
