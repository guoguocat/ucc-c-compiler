#!/usr/bin/perl
use warnings;

sub usage
{
	die "Usage: $0 [-w] [-s] line1 line2... [< to_check]\n"
}

sub trim
{
	my $s = shift;
	$s =~ s/^ +//;
	$s =~ s/ +$//;
	return $s;
}

my($ign_whitespace, $substring) = (0, 0);
for(my @tmp = @ARGV){
	if($_ eq '-w'){
		$ign_whitespace = 1;
	}elsif($_ eq '-s'){
		$substring = 1;
	}else{
		last;
	}
	shift @ARGV;
}

usage() unless @ARGV;

die "$0 needs STDIN from a pipe\n" if -t STDIN;

my @output = map { chomp; $_ } <STDIN>;

if($ign_whitespace){
	@output = grep { !/^[ \t]*$/ } @output;
}

if(@output != @ARGV){
	print "output:\n";
	map { print "  $_\n" } @output;

	die "$0: mismatching output counts\n"
}

for(my $i = 0; $i < @output; ++$i){
	my($line, $search) = ($output[$i], $ARGV[$i]);

	if($ign_whitespace){
		$line   = trim($line);
		$search = trim($search);
	}


	if(($substring ? index($line, $search) == -1 : not($search eq $line))){
		die+($substring ? "no-substring in" : "mismatching")
		. " lines [$i]: '$line' and '$search'\n"
	}
}
