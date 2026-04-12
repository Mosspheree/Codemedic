# Bash completion for codemedic
# Source this file or copy to /etc/bash_completion.d/codemedic

_codemedic() {
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    # Main options
    opts="-y --yes -w --warnings -v --verbose -e --explain-only -b --batch
          -g --git -c -m --json --diff --undo --provider --provider-url
          --log --config --version -h --help --"

    # Context-sensitive completions
    case "$prev" in
        -c)
            COMPREPLY=( $(compgen -W "clang++ g++ clang gcc" -- "$cur") )
            return 0
            ;;
        -m)
            COMPREPLY=( $(compgen -W "claude-sonnet-4-20250514 claude-opus-4-20250514 gpt-4o gpt-4o-mini llama-3.3-70b-versatile llama3 codellama" -- "$cur") )
            return 0
            ;;
        --provider)
            COMPREPLY=( $(compgen -W "anthropic openai groq ollama" -- "$cur") )
            return 0
            ;;
        --log|--config)
            COMPREPLY=( $(compgen -f -- "$cur") )
            return 0
            ;;
        --provider-url)
            return 0
            ;;
    esac

    # Complete options or files
    if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "$opts" -- "$cur") )
    else
        # Complete .c/.cpp/.cc/.cxx files and directories
        COMPREPLY=( $(compgen -f -X '!*.@(c|cpp|cc|cxx|C|h|hpp)' -- "$cur") )
        COMPREPLY+=( $(compgen -d -- "$cur") )
    fi
}

complete -F _codemedic codemedic
complete -F _codemedic ./build/codemedic
