# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(write-stdout) begin
(write-stdout) create "test.txt"
(write-stdout) open "test.txt"
"Amazing Electronic Fact: If you scuffed your feet long enough without
 touching anything, you would build up so many electrons that your
 finger would explode!  But this is nothing to worry about unless you
 have carpeting." --Dave Barry
(write-stdout) end
write-stdout: exit(0)
EOF
pass;
