# tagphotos

## Process

1. The directory /home/joeruff/m/i contains the originals 

   First, find out what file type each original is:

   ```
   find /home/joeruff/m/i -type f -exec file {} \; > /home/joeruff/filetypes.txt
   ```

2. The file /home/joeruff/filetypes.txt has one line per file in /home/joeruff/m/i

   Extract the jpegs and convert them to an index file which is exactly 300 pixels wide

   ```
   grep -i jpeg /home/joeruff/filetypes.txt | sed -e 's/^i\/\([^ ]*\): .* \([0-9]*\)x\([0-9]*\), .*$/\1 \2 \3/' | grep -v "/" | /home/joeruff/m/command
   sort -R commands.sh > commands_rand.sh
   split -l 333333 commands_rand.sh
   nohup ./xaa &
   nohup ./xab &
   nohup ./xac &
   ```

3. The directory /home/joeruff/m/index contains small (300 pixels wide) versions of each of the jpegs (72% of total)

   Move the Orientation tag over to the index files with exiftool 

   ```
   ls -1 /home/joeruff/m/index/ | sort | sed -e 's/^\(.*\)\.jpg/exiftool -tagsFromFile \/home\/joeruff\/m\/i\/\1 -Orientation \/home\/joeruff\/m\/index\/\1.jpg/' | sh &
   find /home/joeruff/m/index -name "*_original" -type f -exec rm {} \;
   ```

4. Some of the small jpegs in /home/joeruff/m/index have been modified to be displayed rotated

   Now create a ppm version of each index file which is exactly 256x256 for comparison (this will distort, but it doesn't matter)

   ```
   find /home/joeruff/m/index/ -type f -name "*.jpg" | sed -e 's/^\/home\/joeruff\/m\/index\/\(.*\)\.jpg$/ffmpeg -y -i \/home\/joeruff\/m\/index\/\1.jpg -s 256x256 \/home\/joeruff\/m\/match\/\1.ppm/' | split -l 333333
   nohup ./xaa &
   nohup ./xab &
   nohup ./xac &
   ```
   
5. The directory /home/joeruff/m/match contains exactly one ppm file for each jpg file in /home/joeruff/m/index

   I thought it would go faster if I concatinated all the ppms into one giant file to compute the vectors

   ```
   cd /home/joeruff/m/
   find match/ -type f | sort | sed -e 's/^\(.*\)$/cat \1 >> matchall/' > concat.sh
   chmod a+x concat.sh 
   nohup ./concat.sh &
   ```
   
6. The single file /home/joeruff/m/matchall contains all the ppm images concatinated

   Finally, compute the vectors from the paper [Fast Multiresolution Image Querying](https://grail.cs.washington.edu/projects/query/mrquery.pdf)

   ```
   gcc -o vector vector.c 
   nohup ./vector &
   ```
   
7. The single file /home/joeruff/m/nohup.out has exactly one line for each index file with three color vectors per line

   I thought the N^2 comparisons between pairs of files would go faster if the data was in a binary file(?)
   ```
   gcc -o convert_to_bin convert_to_bin.c 
    ./convert_to_bin
   ```
   
8. The single file /home/joeruff/server/data.bin has 16 int32_t numbers per color (16 * 4 * 3) for each index file

   I compared all possible combinations of images using 36 threads
   
   ```
   gcc -O3 -pthread -o findmatches findmatches.c
   ./findmatches
   mkdir parallel_matches
   find . -name "matches[0-9][0-9].txt"  -exec mv {} parallel_matches/ \;
   cd parallel_matches/
   cat matches* | sort -n -k 3 > all.txt
   ```
   
9. The single file /home/joeruff/server/parallel_matches/all.txt is a combination of all the outputs of all 36 threads

