# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(buf-hit-rate) begin
(buf-hit-rate) Hit rate increases
(buf-hit-rate) end
EOF
pass;