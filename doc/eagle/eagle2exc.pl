#!/usr/bin/perl -w

if (scalar(@ARGV) < 3) {
    print "\nexcellon.pl <.drl file> <.drd file> <new .drd file>\n\n";
    print "Compiles Eagle's .drl drill definition file with generated\n";
    print ".drd to a new Excellon .drd file containing the drill\n";
    print "definitions. Eagle's .drl file may contain sizes in\n";
    print "mil, in, mm and cm.\n\n";
    exit;
}

open(DRLFILE, "$ARGV[0]") or die "can't open $ARGV[0]";

while (<DRLFILE>)
{
    $_ =~ tr/\r\n//d;
    @drills = (@drills,$_);
}

open(OUTFILE, ">$ARGV[2]") or die "can't open $ARGV[2]";
open(INFILE, "$ARGV[1]") or die "can't open $ARGV[1]";

print OUTFILE "M48\n";

foreach $drill (@drills)
{
    ($nr = $drill) =~ s/.*(T[0-9]*).*/$1/;
    ($size = $drill) =~ s/.*T[0-9]*[^0-9]*([0-9\.]+).*/$1/;
    ($unit = $drill) =~ s/.*[0-9]([a-zA-Z]*).*/$1/;
    if (($unit eq "cm") || ($unit eq "CM")) {
        $size = sprintf("%0.3f",$size /2.54);
    }
    if (($unit eq "mm") || ($unit eq "MM")) {
        $size = sprintf("%0.3f",$size /25.4);
    }
    if (($unit eq "mil") || ($unit eq "MIL")) {
        $size = sprintf("%0.3f",$size / 1000);
    }
    print "found drill nr $nr with size $size and unit $unit\n";
    print OUTFILE "$nr" . "C" . "$size\n";
}

print OUTFILE "G90\nM72\n";

while (<INFILE>)
{
    print OUTFILE $_;
}

close DRLFILE;
close INFILE;
close OUTFILE;

