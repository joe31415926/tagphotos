#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main()
{
    struct {
        int a;
        int b;
        double w;
    } *l = malloc(20334239 * sizeof(l[0]));
    assert(l != NULL);
    
    int i;
    for (i = 0; i < 20334239; i++)
        assert(scanf("%d %d %lg\n", &(l[i].a), &(l[i].b), &(l[i].w)) == 3);
    
    
    printf("%d %d %d %g\n", i, l[20334238].a, l[20334238].b, l[20334238].w);
}