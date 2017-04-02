# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(wait-childterm) begin
(child-simple) run
child-simple: exit(81)
(wait-childterm) Exec Done
(wait-childterm) wait(exec()) = 81
(wait-childterm) end
wait-childterm: exit(0)
EOF
pass;
