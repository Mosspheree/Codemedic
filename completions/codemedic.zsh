#compdef codemedic

# Zsh completion for codemedic
# Install: copy to a directory in $fpath, or source this file

_codemedic() {
    local -a opts
    opts=(
        '-y[Auto-apply patches without prompting]'
        '--yes[Auto-apply patches without prompting]'
        '-w[Also fix warnings]'
        '--warnings[Also fix warnings]'
        '-v[Verbose output]'
        '--verbose[Verbose output]'
        '-e[Explain errors without applying patches]'
        '--explain-only[Explain errors without applying patches]'
        '-b[Batch mode: fix all source files in directory]'
        '--batch[Batch mode: fix all source files in directory]'
        '-g[Auto-commit verified fixes to git]'
        '--git[Auto-commit verified fixes to git]'
        '-c[Compiler to use]:compiler:(clang++ g++ clang gcc)'
        '-m[LLM model]:model:(claude-sonnet-4-20250514 claude-opus-4-20250514 gpt-4o gpt-4o-mini llama-3.3-70b-versatile llama3 codellama)'
        '--json[Machine-readable JSON output]'
        '--diff[Preview patches without applying]'
        '--undo[Restore source files from backups]'
        '--provider[LLM provider]:provider:(anthropic openai groq ollama)'
        '--provider-url[Custom API endpoint URL]:url:'
        '--log[Write fix history to JSON file]:log file:_files'
        '--config[Path to config file]:config:_files'
        '--version[Show version information]'
        '-h[Show help]'
        '--help[Show help]'
    )

    _arguments -s $opts '*:source file:_files -g "*.{c,cpp,cc,cxx,C,h,hpp}(-.)"'
}

_codemedic "$@"
