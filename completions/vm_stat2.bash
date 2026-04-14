_vm_stat2() {
  local cur prev
  cur="${COMP_WORDS[COMP_CWORD]}"
  prev="${COMP_WORDS[COMP_CWORD-1]}"

  if [[ "$prev" == "-c" ]]; then
    return
  fi

  if [[ "$cur" == -* ]]; then
    COMPREPLY=($(compgen -W "-b -k -m -g -a -c" -- "$cur"))
  fi
}
complete -F _vm_stat2 vm_stat2
