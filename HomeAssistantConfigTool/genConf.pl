#!/usr/bin/perl

use strict;
use warnings;

my $mapFile = 'serial.map';

sub getMap($) {
    my $mapFile = slurpFile($_[0]);
    my $map = {};
    for my $line (split(/[\r\n]+/, $mapFile)) {
        next if $line =~ /^\s*#/;
        next if $line !~ /\d+\s*,/;
        chomp $line;
        if ($line =~ /^\s*(\d+)\s*,\s*(.*)$/) {
            $map->{$1} = $2;
        }
    }
    return $map;
}
sub slurpFile($) {
    my $file = $_[0];
    open (my $fh, '<', $file) or die "Can't open $file: $!";
    read $fh, my $file_contents, -s $fh;
    close ($fh) or die "Failed to close $file: $!";
    return $file_contents;
}
my $serialMap = getMap($mapFile);

print qq{\nsensor:\n};
for my $serial (sort {$a<=>$b} keys %$serialMap) {
    my $cname = $serialMap->{$serial};
    print qq{  - platform: mqtt\n};
    print qq{    name: "$cname"\n};
    print qq{    state_topic: "/security/sensors345/$serial/status"\n};
}
print qq{\nbinary_sensor:\n};
print qq{  - platform: mqtt\n};
print qq{    name: 345MHz RX Fault\n};
print qq{    state_topic: "/security/sensors345/rx_status"\n};
print qq{    payload_on: "FAILED"\n};
print qq{    payload_off: "OK"\n};
print qq{    device_class: safety\n};
for my $serial (sort {$a<=>$b} keys %$serialMap) {
    my $cname = $serialMap->{$serial};
    print qq{  - platform: mqtt\n};
    print qq{    name: "$cname Alarm"\n};
    print qq{    state_topic: "/security/sensors345/$serial/alarm"\n};
    print qq{    payload_on: "ALARM"\n};
    print qq{    payload_off: "OK"\n};
    print qq{    device_class: opening\n};
}
exit 0;


__END__