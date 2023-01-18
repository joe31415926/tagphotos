#include <stdio.h>

// find i -type f -exec file {} \; > ~/filetypes.txt &
// grep -i jpeg ~/filetypes.txt | sed -e 's/^i\/\([^ ]*\): .* \([0-9]*\)x\([0-9]*\), .*$/\1 \2 \3/' | grep -v "/" | ~/m/command > ~/m/commands.sh
// sort -R commands.sh > commands_rand.sh 
// split -l 333333 commands_rand.sh 

// joeruff@supercomputer:~/m$ find /home/joeruff/m/i/ -type f | wc -l
// 1335136
// joeruff@supercomputer:~/m$ wc -l /home/joeruff/filetypes.txt 
// 1335136 /home/joeruff/filetypes.txt
// joeruff@supercomputer:~/m$ grep -i jpeg /home/joeruff/filetypes.txt | wc -l
// 963245
// joeruff@supercomputer:~/m$ grep -i jpeg /home/joeruff/filetypes.txt | sed -e 's/^i\/\([^ ]*\): .* \([0-9]*\)x\([0-9]*\), .*$/\1 \2 \3/' | wc -l
// 963245
// joeruff@supercomputer:~/m$ grep -i jpeg /home/joeruff/filetypes.txt | sed -e 's/^i\/\([^ ]*\): .* \([0-9]*\)x\([0-9]*\), .*$/\1 \2 \3/' | grep -v "/" | wc -l
// 963223
// joeruff@supercomputer:~/m$ grep -i jpeg /home/joeruff/filetypes.txt | sed -e 's/^i\/\([^ ]*\): .* \([0-9]*\)x\([0-9]*\), .*$/\1 \2 \3/' | grep -v "/" | /home/joeruff/m/command | wc -l
// 963079
// joeruff@supercomputer:~/m$ find /home/joeruff/m/index/ -type f | wc -l
// 961669
// joeruff@supercomputer:~/m$ 



int main()
{
    char filename[45];
    long w, h;
    while (scanf("%s %ld %ld\n", filename, &w, &h) == 3)
    {
        h = ((long) 300 * h) / w;
        if (h > 10)
            printf("if [[ ! -f /home/joeruff/m/index/%s.jpg ]]; then ffmpeg -y -i /home/joeruff/m/i/%s -s 300x%ld /home/joeruff/m/index/%s.jpg; fi\n", filename, filename, h, filename);
    }
}