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

   Finally, compute the vectors for each ppm file from the paper [Fast Multiresolution Image Querying](https://grail.cs.washington.edu/projects/query/mrquery.pdf) using a program with 25 threads. It usually takes about 24 minutes...

   ```
   cd /home/joeruff/server
   gcc -o vector vector.c -pthread
   time ./vector
   counting the number of files...
   numfiles: 961668
   reading the filenames....
   filenames read
   9 0/38467
   19 0/38467
   18 0/38467
   14 0/38467
   11 0/38467
   ...
   22 38000/38467
   23 38000/38467
   24 38000/38467

   real	23m49.804s
   user	485m8.342s
   sys	1m40.850s
   ```
      
6. The single file /home/joeruff/m/match.bin has a 32 byte filename and 16 int32_t numbers per color (32 + 16 * 4 * 3 = ) for each index file
