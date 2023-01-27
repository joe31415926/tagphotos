# tagphotos

## Process

1. The originals are in /home/joeruff/m/i

   First, find out what file types each original is:
   
   ```
   find /home/joeruff/m/i -type f -exec file {} \; > /home/joeruff/filetypes.txt
   ```
   
2. Extract the jpegs and convert them to an index file which is exactly 

   ```
   grep -i jpeg ~/filetypes.txt | sed -e 's/^i\/\([^ ]*\): .* \([0-9]*\)x\([0-9]*\), .*$/\1 \2 \3/' | grep -v "/" | ~/m/command > ~/m/commands.sh
   ```
