#!/local/bin/bash

results_dir=$1
solution_dir=$2

diff ${results_dir}/case1-cp.img.txt ${solution_dir}/case1-cp.img.txt
diff ${results_dir}/case2-cp-large.img.txt ${solution_dir}/case2-cp-large.img.txt
diff ${results_dir}/case3-cp-rm-dir.img.txt ${solution_dir}/case3-cp-rm-dir.img.txt
diff ${results_dir}/case4-mkdir.img.txt ${solution_dir}/case4-mkdir.img.txt
diff ${results_dir}/case5-mkdir-2.img.txt ${solution_dir}/case5-mkdir-2.img.txt
diff ${results_dir}/case6-ln-hard.img.txt ${solution_dir}/case6-ln-hard.img.txt
diff ${results_dir}/case7-ln-soft.img.txt ${solution_dir}/case7-ln-soft.img.txt
diff ${results_dir}/case8-rm.img.txt ${solution_dir}/case8-rm.img.txt
diff ${results_dir}/case9-rm-2.img.txt ${solution_dir}/case9-rm-2.img.txt
diff ${results_dir}/case10-rm-3.img.txt ${solution_dir}/case10-rm-3.img.txt
diff ${results_dir}/case11-rm-large.img.txt ${solution_dir}/case11-rm-large.img.txt
diff ${results_dir}/case12-rs.img.txt ${solution_dir}/case12-rs.img.txt
diff ${results_dir}/case13-rs-2.img.txt ${solution_dir}/case13-rs-2.img.txt
diff ${results_dir}/case14-rs-large.img.txt ${solution_dir}/case14-rs-large.img.txt
diff ${results_dir}/case15-checker.img.txt ${solution_dir}/case15-checker.img.txt