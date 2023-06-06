# simple-shell
### Commands
#### Supports all Unix commands using  `execvp()`
### Supported Metacharacters

| Metacharacter | Feature | Example |
| --- | --- | --- |
| `>` | Output redirection to specified output file. Output file is truncated. | `echo toto > output` |
| `>>` | Output reditrection to specified output file. Output file is appended to. | `echo toto >> output` |
| `\|` | Pipe sign allowing multiple commands to be connected to one another in order to build more complex jobs | `echo Hello world \| grep Hello \| wc -l` |
| `&` | Indicates that the specified job should be executed in the background | `sleep 1&` |

#### For more information on implementation read `REPORT.md`
#### Use the makefile to run

1. Download all files from frontend folder.
2. Download our effnetLArch and effnetMArch model [here](https://drive.google.com/drive/folders/1xze_Abo72s9FCRjBV22VuelzZN2LqrTe?usp=sharing) into the frontend folder.
3. Download dependicies
  - pip install flask
  - pip install torch
  - pip install torchvision
4. Run server using python main.py
5. Go to http://localhost:81/ (Testing photos can be found on the [google drive](https://drive.google.com/drive/folders/1xze_Abo72s9FCRjBV22VuelzZN2LqrTe?usp=sharing))
