# -*- perl -*-
use strict;
use warnings;
use tests::tests;
use tests::random;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(my-test-2) begin
(my-test-2) create "a"
(my-test-2) open "a"
(my-test-2) 100 reads in 100 writes
(my-test-2) 73 writes in 100 writes
(my-test-2) end
EOF
pass;