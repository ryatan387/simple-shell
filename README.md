# simple-shell
###Commands
#### Supports all Unix commands using  `execvp()`
###Supported Metacharacters

| Metacharacter | Feature | Example |
| --- | --- | --- |
| `>` | Output redirection to specified output file. Output file is truncated. | `echo toto > output` |
| `>>` | Output reditrection to specified output file. Output file is appended to. | `echo toto >> output` |
| `\|` | Pipe sign allowing multiple commands to be connected to one another in order to build more complex jobs | `echo Hello world \| grep Hello \| wc -l` |
| `&` | Indicates that the specified job should be executed in the background | `sleep 1&` |

####For more information on implementation read `REPORT.md`
