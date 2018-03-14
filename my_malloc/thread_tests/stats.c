#include <stdlib.h>
#include <stdio.h>








int main(void){

  unsigned long avg_time_sum = 0;
  unsigned long num_times = 0;
  

  FILE * f = fopen("report_output", "r");
  
  char * line = NULL;
  
  ssize_t len = 0;
  size_t sz = 0;
  while (len = getline(&line, &sz, f) >= 0){
    printf("%s\n", line);
  }
  

  fclose(f);








  return EXIT_SUCCESS;
}
