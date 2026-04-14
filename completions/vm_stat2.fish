complete --command vm_stat2 --no-files

complete --command vm_stat2 --condition '__fish_not_contain_opt -s b -s k -s m -s g' --short-option b --description 'Display values in bytes'
complete --command vm_stat2 --condition '__fish_not_contain_opt -s b -s k -s m -s g' --short-option k --description 'Display values in kilobytes'
complete --command vm_stat2 --condition '__fish_not_contain_opt -s b -s k -s m -s g' --short-option m --description 'Display values in megabytes'
complete --command vm_stat2 --condition '__fish_not_contain_opt -s b -s k -s m -s g' --short-option g --description 'Display values in gigabytes'
complete --command vm_stat2 --condition '__fish_not_contain_opt -s a'                --short-option a --description 'Show all details (verbose)'
complete --command vm_stat2 --condition '__fish_not_contain_opt -s c'                --short-option c --description 'Number of times to poll' --require-parameter
