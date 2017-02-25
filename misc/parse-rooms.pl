#!/usr/bin/perl

use strict;
use warnings;
use Text::CSV;

use constant {
  EXIT_FLAG_EXITSTR   => (1<<0),
  EXIT_FLAG_DOOR      => (1<<1),
  EXIT_FLAG_SECRET    => (1<<2),
  EXIT_FLAG_ONEWAY    => (1<<3),
  EXIT_FLAG_KEYREQ    => (1<<4),
  EXIT_FLAG_COMMAND   => (1<<5),
  EXIT_FLAG_TRAP      => (1<<6),
  EXIT_FLAG_DETOUR    => (1<<7),
  EXIT_FLAG_BLOCKED   => (1<<8),
  EXIT_FLAG_ROOMCLEAR => (1<<9),
};

die "usage $0 [filename]\n" unless @ARGV == 1;

my $filename = shift;
my $csv = Text::CSV->new();

open CSV, $filename or die "unable to open '$filename' for reading: $!\n";

while (<CSV>) {

    die "error reading input file" unless $csv->parse($_);

    my @fields = $csv->fields();
    my $id = sprintf "\"%s/%s\"", $fields[0], $fields[1];
    my $name = "$fields[2]";
    my ($exits, $flags, $session) = (0, 0, 0);
    my ($x, $y, $z) = (0, 0, 0);

    my @exit_info; # id, special_str, direction, flags

    for (my $i = 9; $i < 19; $i++) {
        if ($fields[$i] =~ /^(\d+)\/(\d+)\s?(.*)$/) {
            my ($map, $room, $special) = ($1, $2, $3);
            my $exit_id = sprintf "%s/%s", $map, $room;
            my $exit_index = 1 << ($i-8);
            my $exit_flags = 0;
            my $custom = "";
            my $visible = 1;

            if ($special) {
                if ($special =~ /\(Door.*\)/) {
                    $exit_flags += EXIT_FLAG_DOOR;
                }
                elsif (/\(Text: (.*)\)/) {
                    $custom = "unhandled";
                    $exit_flags += EXIT_FLAG_EXITSTR;
                    $visible = 0;
                }
                else {
                    $custom = "unhandled";
                }
            }

            $exits += $exit_index if $visible;

            push @exit_info, "\"$exit_id\", \"$custom\", $exit_index, $exit_flags";
        }
    }

    print "$id, \"$name\", $exits, $flags, $x, $y, $z, $session\n";
    print "\t$_\n" for (@exit_info);
    print "\n";
}

close CSV;
