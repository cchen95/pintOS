# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(my-test-1) begin
(my-test-1) Hit rate increases
(my-test-1) end
EOF
pass;