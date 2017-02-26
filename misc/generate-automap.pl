#!/usr/bin/perl

use strict;
use warnings;
use Text::CSV;
use Data::Dumper;

use constant {
    EXIT_FLAG_EXITSTR   => (1<<0),
    EXIT_FLAG_DOOR      => (1<<1),
    EXIT_FLAG_SECRET    => (1<<2),
    EXIT_FLAG_ONEWAY    => (1<<3),
    EXIT_FLAG_KEYREQ    => (1<<4),
    EXIT_FLAG_ITEMREQ   => (1<<5),
    EXIT_FLAG_COMMAND   => (1<<6),
    EXIT_FLAG_TRAP      => (1<<7),
    EXIT_FLAG_DETOUR    => (1<<8),
    EXIT_FLAG_BLOCKED   => (1<<9),
    EXIT_FLAG_ROOMCLEAR => (1<<10),
    EXIT_FLAG_TOLL      => (1<<11),

    ROOM_FLAG_REGEN     => (1<<0), # monster regen
    ROOM_FLAG_SYNC      => (1<<1), # sync the automap
    ROOM_FLAG_NOREST    => (1<<2), # do not rest here
    ROOM_FLAG_STASH     => (1<<3), # stash treasure here
    ROOM_FLAG_FULL_HP   => (1<<4), # HP must be full to enter
    ROOM_FLAG_FULL_MA   => (1<<5), # MA must be full to enter
    ROOM_FLAG_NOENTER   => (1<<6), # do not enter this room
    ROOM_FLAG_NOROAM    => (1<<7), # do not roam though this room
};

my %mapExit = (
    'n'  => (1<<1), 's'  => (1<<2), 'e'  => (1<<3), 'w'  => (1<<4),
    'ne' => (1<<5), 'nw' => (1<<6), 'se' => (1<<7), 'sw' => (1<<8),
    'u'  => (1<<9), 'd'  => (1<<10)
);

die "usage $0 [rooms.csv] [items.csv]\n" unless @ARGV == 2;

my ($room_csv, $item_csv) = @ARGV;
my $csv = Text::CSV->new();
my %rooms;
my %items;
my %unique;

open CSV, $item_csv or die "unable to open '$item_csv': $!\n";

while (<CSV>) {
    die "error reading input file" unless $csv->parse($_);

    my @fields = $csv->fields();
    my ($id, $name) = @fields;
    $items{$id} = $name;
}

close CSV;

open CSV, $room_csv or die "unable to open '$room_csv': $!\n";

while (<CSV>) {

    die "error reading input file" unless $csv->parse($_);

    my @fields = $csv->fields();
    my $id = sprintf "%s/%s", $fields[0], $fields[1];
    my $name = "$fields[2]";
    my ($exits, $flags) = (0, 0);

    my @exit_info; # id, special_str, direction, flags
    my %special;

    die "duplicate room definition found for '$id'!\n" if exists $rooms{$id};

    $unique{$name}++;

    for (my $i = 9; $i < 19; $i++) {
        next if $fields[$i] eq '0';

        if ($fields[$i] =~ /^(\d+)\/(\d+)\s?(.*)$/) {
            my ($map, $room, $special) = ($1, $2, $3);
            my $exit_id = sprintf "%s/%s", $map, $room;
            my $exit_index = 1 << ($i-8);
            my $exit_flags = 0;
            my $custom = "";
            my $required = "";
            my $visible = 1;

            if ($special) {
                if ($special =~ /\(Door.*\)/) {
                    $exit_flags |= EXIT_FLAG_DOOR;
                }
                elsif ($special =~ /Key: (\d+)/) {
                    $exit_flags |= EXIT_FLAG_KEYREQ;
                    $required = $items{$1} || 'undef';
                }
                elsif ($special =~ /Item: (\d+)/) {
                    $exit_flags |= EXIT_FLAG_ITEMREQ;
                    $required = $items{$1} || 'undef';
                }
                elsif ($special =~ /Searchable/) {
                    $exit_flags |= EXIT_FLAG_SECRET;
                    $visible = 0;
                }
                elsif ($special =~ /^\(Text: ([a-z, ]+)\)$/) {
                    my @command = split /,/, $1;
                    $custom = $command[0];
                    $exit_flags |= EXIT_FLAG_EXITSTR;
                    $visible = 0;
                }
                elsif ($special =~ /Needs \d+ Actions/) {
                    $exit_flags |= EXIT_FLAG_SECRET;
                    $exit_flags |= EXIT_FLAG_COMMAND;
                    $visible = 0;
                }
                elsif ($special =~ /Passable/) {
                    $visible = 0;
                }
                elsif ($special =~ /Trap/) {
                    $exit_flags |= EXIT_FLAG_TRAP;
                }
                elsif ($special =~ /Toll: (\d+)/) {
                    $exit_flags |= EXIT_FLAG_TOLL;
                    $required = $1;
                }
                elsif ($special =~ /Cast|Timed|Unknown/) {
                    # not sure - not used for now
                }
                elsif ($special =~ /Ability|Alignment|Level|Class|Race/) {
                    # don't do anything with these now
                }
                else {
                    # print "unhandled $id ($name) -> $map/$room |$special|\n";
                }
            }

            $exits += $exit_index if $visible;

            push @exit_info, {
                exit_id    => $exit_id,
                exit_index => $exit_index,
                exit_flags => $exit_flags,
                custom     => $custom,
                required   => $required
            };
        }
        elsif ($fields[$i] =~ /^Action.*\[on the (\w+) exit.+\]: ([A-Za-z0-9', ]+[A-Za-z0-9'])/) {
            my ($direction, $commands) = ($mapExit{lc $1}, $2);

            my @tmp = split /,/, $commands;
            push @{ $special{$direction}{actions} }, $tmp[0];

            if ($fields[$i] =~ /Item: (\d+)/) {
                $special{$direction}{required} = $items{$1} || 'undef';
            }
        }
        else {
            #printf "$id - unhandled -> '%s'\n", $fields[$i];
        }
    }

    foreach my $exit_info (@exit_info) {
        my $index = $exit_info->{exit_index};
        if (exists $special{$index}) {
            if (exists $special{$index}{actions}) {
                $exit_info->{custom} = join ',', @{ $special{$index}{actions} };
            }
            if (exists $special{$index}{required}) {
                $exit_info->{required} = $special{$index}{required};
            }
        }
    }

    $rooms{$id} = {
        id        => $id,
        name      => $name,
        exits     => $exits,
        flags     => $flags,
        exit_info => \@exit_info
    };
}

close CSV;

foreach (keys %rooms) {
    my $room = $rooms{$_};

    $room->{flags} |= ROOM_FLAG_SYNC if $unique{$room->{name}} == 1;

    printf "%s, \"%s\", %d, %d, 0, 0, 0, 0\n",
        $room->{id}, $room->{name}, $room->{exits}, $room->{flags};

    foreach my $exit_info (@{ $room->{exit_info} }) {
        printf "\t%s, \"%s\", \"%s\", %d, %d\n",
            $exit_info->{exit_id},
            $exit_info->{custom},
            $exit_info->{required},
            $exit_info->{exit_index},
            $exit_info->{exit_flags};
    }

    print "\n";
}
