#include "methylation.h"


//------------------------------------------------------------------------------------

void replace(char * refs, int len, int type) {
  char c1, c2;

  switch (type) {
    case ACGT:
      // Case with no transformation required
      return;

    case AGT:
      c1 = 'C';
      c2 = 'T';
      break;

    case ACT:
      c1 = 'G';
      c2 = 'A';
      break;

    case AT:
      // Apply both transformations, and end the execution
      replace(refs, len, 1);
      replace(refs, len, 2);
      return;

    default:
      // If the type value is not recognised, nothing is done
      return;
  }

  // Transforms the refs sequence, replacing the caracter c1 with the caracter c2
  for (int j = 0; j < len; j++) {
    if (refs[j] == c1) {
      refs[j] = c2;
    }
  }

  return;
}

//------------------------------------------------------------------------------------

char complement (char c) {
  // Return the complementary base
  switch (c) {
    case 'A':
      return 'T';
    case 'T':
      return 'A';
    case 'C':
      return 'G';
    case 'G':
      return 'C';
    default:
      return c;
  }
}

//------------------------------------------------------------------------------------

void rev_comp(char *orig, char *dest, int len) {
  // Put the reverse complementary sequence of orig in dest
  for (int i = 0; i < len; i++) {
    dest[len - i - 1] = complement(orig[i]);
  }

  dest[len] = '\0';
}

//------------------------------------------------------------------------------------

void comp(char *seq, int len) {
  // obtain the complementary sequence of seq
  for (int i = 0; i < len - 1; i++) {
    seq[i] = complement(seq[i]);
  }
}


//------------------------------------------------------------------------------------

void cpy_transform_array_bs(array_list_t *src, array_list_t *dest_ct, array_list_t *dest_ct_rev, array_list_t *dest_ga, array_list_t *dest_ga_rev) {
  size_t num_reads = array_list_size(src);
  fastq_read_t *fq_read_src;
  fastq_read_t *fq_read_dest;
  fastq_read_t *fq_read_tmp;

  // Read element by element in the array, transform each one, and put in the new arrays
  for (size_t i = 0; i < num_reads; i++) {
    fq_read_src  = (fastq_read_t *) array_list_get(i, src);

    fq_read_dest = fastq_read_dup(fq_read_src);
    replace(fq_read_dest->sequence, fq_read_dest->length, AGT);
    array_list_insert(fq_read_dest, dest_ct);

    fq_read_tmp = fastq_read_dup(fq_read_src);
    rev_comp(fq_read_dest->sequence, fq_read_tmp->sequence, fq_read_dest->length);
    array_list_insert(fq_read_tmp, dest_ct_rev);

    fq_read_dest = fastq_read_dup(fq_read_src);
    replace(fq_read_dest->sequence, fq_read_dest->length, ACT);
    array_list_insert(fq_read_dest, dest_ga);

    fq_read_tmp = fastq_read_dup(fq_read_src);
    rev_comp(fq_read_dest->sequence, fq_read_tmp->sequence, fq_read_dest->length);
    array_list_insert(fq_read_tmp, dest_ga_rev);
  }
}

//------------------------------------------------------------------------------------

void revert_mappings_seqs(array_list_t **src1, array_list_t **src2, array_list_t *orig) {
  size_t num_mappings;
  alignment_t *align_tmp;
  fastq_read_t *fastq_orig;
  size_t num_reads = array_list_size(orig);

  // Go over all the sequences
  for (size_t i = 0; i < num_reads; i++) {
    fastq_orig  = (fastq_read_t *) array_list_get(i, orig);
 
    // Go over all the alignments in list 1
    num_mappings = array_list_size(src1[i]);
    
    for (size_t j = 0; j < num_mappings; j++) {
      align_tmp  = (alignment_t *) array_list_get(j, src1[i]);

      // Free existing memory and copy the original
      if (align_tmp->sequence != NULL) {
        free(align_tmp->sequence);
      }

      align_tmp->sequence = strdup(fastq_orig->sequence);
    }

    // Go over all the alignments in list 2
    num_mappings = array_list_size(src2[i]);

    for (size_t j = 0; j < num_mappings; j++) {
      align_tmp  = (alignment_t *) array_list_get(j, src2[i]);

      // Free existing memory and copy the original
      if (align_tmp->sequence != NULL) {
        free(align_tmp->sequence);
      }

      align_tmp->sequence = strdup(fastq_orig->sequence);
    }
  }
}

//------------------------------------------------------------------------------------

char *obtain_seq(alignment_t *alig, fastq_read_t * orig) {
  char *read = orig->sequence;
  char *cigar = strdup(alig->cigar);
  int num;
  char car;
  int cont, pos, pos_read;
  int operations;
  int len = strlen(cigar) - 1;
  char *seq = (char *)calloc(1024, sizeof(char));
  
  pos = 0;
  pos_read = 0;
  
  for (operations = 0; operations < alig->num_cigar_operations; operations++) {
    sscanf(cigar, "%i%c%s", &num, &car, cigar);

    if (car == 'M' || car == '=' || car == 'X') {
      for (cont = 0; cont < num; cont++, pos++, pos_read++) {
	      seq[pos] = read[pos_read];
      }
    } else {
      if (car == 'D' || car == 'N') {
	      pos_read += num - 1;
      } else {
        if (car == 'I' || car == 'H' || car == 'S') {
          for (cont = 0; cont < num; cont++, pos++) {
            seq[pos] = '-';
          }
        }
      }
    }
  }

  seq[pos] = '\0';

  free(cigar);
  return seq;
}

//------------------------------------------------------------------------------------

void metil_file_init(metil_file_t *metil_file, char *dir, genome_t *genome) {
  char *name_tmp = malloc(128 * sizeof(char));

  int file_error;

  sprintf(name_tmp, "%s/CpG_context.txt", dir);
  metil_file->filenameCpG = strdup(name_tmp);
  sprintf(name_tmp, "%s/CHG_context.txt", dir);
  metil_file->filenameCHG = strdup(name_tmp);
  sprintf(name_tmp, "%s/CHH_context.txt", dir);
  metil_file->filenameCHH = strdup(name_tmp);
  sprintf(name_tmp, "%s/MUT_context.txt", dir);
  metil_file->filenameMUT = strdup(name_tmp);
  sprintf(name_tmp, "%s/Statistics.txt", dir);
  metil_file->filenameSTAT = strdup(name_tmp);


  metil_file->genome = genome;

  metil_file->CpG  = fopen(metil_file->filenameCpG,  "w");
  metil_file->CHG  = fopen(metil_file->filenameCHG,  "w");
  metil_file->CHH  = fopen(metil_file->filenameCHH,  "w");
  metil_file->MUT  = fopen(metil_file->filenameMUT,  "w");
  metil_file->STAT = fopen(metil_file->filenameSTAT, "w");

  FILE *a;

  a = metil_file->CpG;
  file_error = fprintf(a, "File for Cytosines in CpG context\n\n");

  if (file_error < 0) {
    printf("Error al escribir\n");
    exit(-1);
  }

  a = metil_file->CHG;
  file_error = fprintf(a, "File for Cytosines in CHG context\n\n");

  if (file_error < 0) {
    printf("Error al escribir\n");
    exit(-1);
  }

  a = metil_file->CHH;
  file_error = fprintf(a, "File for Cytosines in CHH context\n\n");

  if (file_error < 0) {
    printf("Error al escribir\n");
    exit(-1);
  }

  a = metil_file->MUT;
  file_error = fprintf(a, "File for Cytosines mutated\n\n");

  if (file_error < 0) {
    printf("Error al escribir\n");
    exit(-1);
  }

  a = metil_file->STAT;
  file_error = fprintf(a, "File for Methylation Statistics\n\n");

  if (file_error < 0) {
    printf("Error al escribir\n");
    exit(-1);
  }

  free(name_tmp);

  metil_file->CpG_methyl   = 0;
  metil_file->CpG_unmethyl = 0;
  metil_file->CHG_methyl   = 0;
  metil_file->CHG_unmethyl = 0;
  metil_file->CHH_methyl   = 0;
  metil_file->CHH_unmethyl = 0;
  metil_file->MUT_methyl   = 0;
  metil_file->num_bases    = 0;
}

//------------------------------------------------------------------------------------

void metil_file_free(metil_file_t *metil_file) {
  free(metil_file->filenameCpG);
  free(metil_file->filenameCHG);
  free(metil_file->filenameCHH);
  free(metil_file->filenameMUT);
  free(metil_file->filenameSTAT);

  if (metil_file->CpG  != NULL) fclose(metil_file->CpG);
  if (metil_file->CHG  != NULL) fclose(metil_file->CHG);
  if (metil_file->CHH  != NULL) fclose(metil_file->CHH);
  if (metil_file->MUT  != NULL) fclose(metil_file->MUT);
  if (metil_file->STAT != NULL) fclose(metil_file->STAT);

  free(metil_file);
}

//====================================================================================

void remove_duplicates(size_t reads, array_list_t **list, array_list_t **list2) {
  size_t num_items, num_items2;
  alignment_t *alig, *alig2;

  for (size_t i = 0; i < reads; i++) {
    num_items = array_list_size(list[i]);

    for (size_t j = 0; j < num_items; j++) {
      alig = (alignment_t *) array_list_get(j, list[i]);

      if (alig != NULL && alig->is_seq_mapped) {
        num_items2 = array_list_size(list2[i]);

        for (size_t k = 0; k < num_items2; k++) {
          alig2 = (alignment_t *) array_list_get(k, list2[i]);

          if (alig->position      == alig2->position
              && alig->chromosome == alig2->chromosome
              && alig->seq_strand == alig2->seq_strand
              && alig->num_cigar_operations == alig2->num_cigar_operations) {
            alig2 = (alignment_t *) array_list_remove_at(k, list2[i]);
            alignment_free(alig2);
            k--;
            num_items2 = array_list_size(list2[i]);
          }
        }
      }
    }
  }
}

//------------------------------------------------------------------------------------

int methylation_status_report(sw_server_input_t* input, batch_t *batch) {

  LOG_DEBUG("========= METHYLATION STATUS REPORT START =========\n");

  mapping_batch_t *mapping_batch = (mapping_batch_t *) batch->mapping_batch;
  array_list_t **mapping_lists;
  size_t num_items;
  size_t num_reads = array_list_size(mapping_batch->fq_batch);
  genome_t *genome = input->genome_p;
  fastq_read_t *orig;

  struct timeval methylation_time_start, methylation_time_end;
  float methylation_time = 0.0f;

  if (time_on) {
    start_timer(methylation_time_start);
  }

  // Inicializar listas para guardar datos de c's metiladas/no metiladas
  bs_context_t *bs_context = bs_context_new(10000);
  mapping_batch->bs_context = bs_context;

  remove_duplicates(num_reads, mapping_batch->mapping_lists, mapping_batch->mapping_lists2);
  
  for (int k = 0; k < 2; k++) {
    mapping_lists = (k == 0) ? mapping_batch->mapping_lists : mapping_batch->mapping_lists2;
    
    for (size_t i = 0; i < num_reads; i++) {
      num_items = array_list_size(mapping_lists[i]);
      
      // Mapped or not mapped ?
      if (num_items != 0) {
	      add_metilation_status(mapping_lists[i], bs_context, genome, mapping_batch->fq_batch, i, k);
      }
    }
  }

  if (time_on) {
    stop_timer(methylation_time_start, methylation_time_end, methylation_time);
    timing_add(methylation_time, METHYLATION_REP_TIME, timing);
  }

  return CONSUMER_STAGE;
}

//------------------------------------------------------------------------------------

void add_metilation_status(array_list_t *array_list, bs_context_t *bs_context, genome_t * genome, array_list_t * orig_seq, size_t index, int conversion) {
  size_t num_items = array_list_size(array_list);
  size_t len, end, start;

  alignment_t *alig;
  fastq_read_t *orig;
  metil_data_t *metil_data;

  char *seq, *gen;
  char *new_stage;
  
  int new_strand;
  int write_file = 1;

  orig = (fastq_read_t *) array_list_get(index, orig_seq);

  for (size_t j = 0; j < num_items; j++) {
    alig = (alignment_t *) array_list_get(j, array_list);

    if (alig != NULL && alig->is_seq_mapped) {
      seq = obtain_seq(alig, orig);

      if (alig->seq_strand == 1) {
        char *seq_dup = strdup(seq);
        rev_comp(seq_dup, seq, orig->length);
        free(seq_dup);
      }

      // Increase de counter number of bases
      len = orig->length;
      gen = (char *)calloc(len + 5, sizeof(char));

      start = alig->position + 1;
      end = start + len + 4;

      if (end >= genome->chr_size[alig->chromosome]) {
        end = genome->chr_size[alig->chromosome] - 1;
      }

      genome_read_sequence_by_chr_index(gen, alig->seq_strand, alig->chromosome, &start, &end, genome);

      for (size_t i = 0; i < len; i++) {
        if ((conversion == 1 && alig->seq_strand == 0) || (conversion == 0 && alig->seq_strand == 1)) {
          // Methylated/unmethylated cytosines are located in the same strand as the alignment
          if (gen[i] == 'C') {
            if (gen[i + 1] == 'G') {
              if (seq[i] == 'C') {
                if (write_file == 1) {
                  postprocess_bs(alig->query_name, '+', alig->chromosome, start + i, 'Z', alig->seq_strand, 0, bs_context->context_CpG);
                }

                bs_context->CpG_methyl++;
              } else if (seq[i] == 'T') {
                if (write_file == 1) {
                  postprocess_bs(alig->query_name, '-', alig->chromosome, start + i, 'z', alig->seq_strand, 0,bs_context->context_CpG);
                }

                bs_context->CpG_unmethyl++;
              } else {
                if (write_file == 1) {
                  postprocess_bs(alig->query_name, '.', alig->chromosome, start + i, 'M', alig->seq_strand, 3,bs_context->context_MUT);
                }
                
                bs_context->MUT_methyl++;
              }
            } else {
              if (gen[i + 2] == 'G') {
                if (seq[i] == 'C') {
                  if (write_file == 1) {
                    postprocess_bs(alig->query_name, '+', alig->chromosome, start + i, 'X', alig->seq_strand, 1,bs_context->context_CHG);
                  }

                  bs_context->CHG_methyl++;
                } else if (seq[i] == 'T') {
                  if (write_file == 1) {
                    postprocess_bs(alig->query_name, '-', alig->chromosome, start + i, 'x', alig->seq_strand, 1,bs_context->context_CHG);
                  }

                  bs_context->CHG_unmethyl++;
                } else {
                  if (write_file == 1) {
                    postprocess_bs(alig->query_name, '.', alig->chromosome, start + i, 'M', alig->seq_strand, 3,bs_context->context_MUT);
                  }

                  bs_context->MUT_methyl++;
                }
              } else {
                if (seq[i] == 'C') {
                  if (write_file == 1) {
                    postprocess_bs(alig->query_name, '+', alig->chromosome, start + i, 'H', alig->seq_strand, 2,bs_context->context_CHH);
                  }

                  bs_context->CHH_methyl++;
                } else if (seq[i] == 'T') {
                  if (write_file == 1) {
                    postprocess_bs(alig->query_name, '-', alig->chromosome, start + i, 'h', alig->seq_strand, 2,bs_context->context_CHH);
                  }

                  bs_context->CHH_unmethyl++;
                } else {
                  if (write_file == 1) {
                    postprocess_bs(alig->query_name, '.', alig->chromosome, start + i, 'M', alig->seq_strand, 3,bs_context->context_MUT);
                  }
                      
                  bs_context->MUT_methyl++;
                }
              }
            }
          }
        } else {
          // Methylated/unmethylated cytosines are located in the other strand
          if (alig->seq_strand == 0) {
            new_strand = 1;
          } else {
            new_strand = 0;
          }
          
          if (gen[i+2] == 'G') {
            if (gen[i + 1] == 'C') {
              if (seq[i] == 'G') {
                if (write_file == 1) {
                  postprocess_bs(alig->query_name, '+', alig->chromosome, start + i, 'Z', new_strand, 0,bs_context->context_CpG);
                }

                bs_context->CpG_methyl++;
              } else if (seq[i] == 'A') {
                if (write_file == 1) {
                  postprocess_bs(alig->query_name, '-', alig->chromosome, start + i, 'z', new_strand, 0,bs_context->context_CpG);
                }

                bs_context->CpG_unmethyl++;
              } else {
                if (write_file == 1) {
                  postprocess_bs(alig->query_name, '.', alig->chromosome, start + i, 'M', alig->seq_strand, 3,bs_context->context_MUT);
                }
                
                bs_context->MUT_methyl++;
              }
            } else {
              if (gen[i] == 'C') {
                if (seq[i] == 'G') {
                  if (write_file == 1) {
                    postprocess_bs(alig->query_name, '+', alig->chromosome, start + i, 'X', new_strand, 1,bs_context->context_CHG);
                  }
                      
                  bs_context->CHG_methyl++;
                } else if (seq[i] == 'A') {
                  if (write_file == 1) {
                    postprocess_bs(alig->query_name, '-', alig->chromosome, start + i, 'x', new_strand, 1,bs_context->context_CHG);
                  }

                  bs_context->CHG_unmethyl++;
                } else {
                  if (write_file == 1) {
                    postprocess_bs(alig->query_name, '.', alig->chromosome, start + i, 'M', alig->seq_strand, 3,bs_context->context_MUT);
                  }

                  bs_context->MUT_methyl++;
                }
              } else {
                if (seq[i] == 'G') {
                  if (write_file == 1) {
                    postprocess_bs(alig->query_name, '+', alig->chromosome, start + i, 'H', new_strand, 2,bs_context->context_CHH);
                  }

                  bs_context->CHH_methyl++;
                } else if (seq[i] == 'A') {
                  if (write_file == 1) {
                    postprocess_bs(alig->query_name, '-', alig->chromosome, start + i, 'h', new_strand, 2,bs_context->context_CHH);
                  }

                  bs_context->CHH_unmethyl++;
                } else {
                  if (write_file == 1) {
                    postprocess_bs(alig->query_name, '.', alig->chromosome, start + i, 'M', alig->seq_strand, 3,bs_context->context_MUT);
                  }

                  bs_context->MUT_methyl++;
                }
              }
            }
          }
        }
      }

      if (seq) {
        free(seq);
      }

      if (gen) {
        free(gen);
      }
    }
  }
}

//------------------------------------------------------------------------------------

void add_metilation_status_bin(array_list_t *array_list, bs_context_t *bs_context,
			       unsigned long long **gen_binCT, unsigned long long **gen_binGA,
			       array_list_t * orig_seq, size_t index, int conversion) {
  size_t num_items = array_list_size(array_list);
  size_t len, end, start;

  alignment_t *alig;
  fastq_read_t *orig;
  metil_data_t *metil_data;

  char *seq, *gen;
  char posit = '+', negat = '-';
  char *new_stage;

  int new_strand;
  int write_file = 1;

  orig = (fastq_read_t *) array_list_get(index, orig_seq);

  for (size_t j = 0; j < num_items; j++) {
    alig = (alignment_t *) array_list_get(j, array_list);

    if (alig != NULL && alig->is_seq_mapped) {
      seq = obtain_seq(alig, orig);

      if (alig->seq_strand == 1) {
        char *seq_dup = strdup(seq);
        rev_comp(seq_dup, seq, orig->length);
        free(seq_dup);
      }

      len = orig->length;
      start = alig->position;
      end = start + len;

      if ((conversion == 1 && alig->seq_strand == 0) || (conversion == 0 && alig->seq_strand == 1)) {
        search_methylation(alig->chromosome, start, end, gen_binCT, seq, bs_context, '+', 0, alig->query_name);
      } else {
        search_methylation(alig->chromosome, start, end, gen_binCT, seq, bs_context, '-', 1, alig->query_name);
      }

      if (seq) free(seq);
      if (gen) free(gen);
    }
  }
}

//------------------------------------------------------------------------------------

void write_bs_context(metil_file_t *metil_file, bs_context_t *bs_context) {
  array_list_t *context_CpG = bs_context->context_CpG;
  array_list_t *context_CHG = bs_context->context_CHG;
  array_list_t *context_CHH = bs_context->context_CHH;
  array_list_t *context_MUT = bs_context->context_MUT;

  size_t num_items, num_reads;
  char *bs_seq;
  int file_error;
  metil_data_t *metil_data;

  FILE * CpG = metil_file->CpG;
  FILE * CHG = metil_file->CHG;
  FILE * CHH = metil_file->CHH;
  FILE * MUT = metil_file->MUT;

  metil_file->CpG_methyl   += bs_context->CpG_methyl;
  metil_file->CpG_unmethyl += bs_context->CpG_unmethyl;
  metil_file->CHG_methyl   += bs_context->CHG_methyl;
  metil_file->CHG_unmethyl += bs_context->CHG_unmethyl;
  metil_file->CHH_methyl   += bs_context->CHH_methyl;
  metil_file->CHH_unmethyl += bs_context->CHH_unmethyl;
  metil_file->MUT_methyl   += bs_context->MUT_methyl;
  metil_file->num_bases    += bs_context->num_bases;

  if (CpG == NULL) {
    printf("reopen CpG file\n");
    CpG = fopen(metil_file->filenameCpG, "a");
  }

  if (CHG == NULL) {
    printf("reopen CHG file\n");
    CHG = fopen(metil_file->filenameCHG, "a");
  }

  if (CHH == NULL) {
    printf("reopen CHH file\n");
    CHH = fopen(metil_file->filenameCHH, "a");
  }

  if (MUT == NULL) {
    printf("reopen CHH file\n");
    MUT = fopen(metil_file->filenameMUT, "a");
  }

  if (context_CpG != NULL) {
    num_items = array_list_size(context_CpG);

    for (int i = num_items - 1; i >= 0; i--) {
      metil_data = (metil_data_t *)array_list_get(i, context_CpG);
      file_error = fprintf(CpG, "%s\t%c\t%i\t%lu\t%c\t%i\n",
                          metil_data->query_name, metil_data->status,
                          metil_data->chromosome, metil_data->start,
                          metil_data->context, metil_data->strand);
      metil_data_free(metil_data);
      
      if (file_error < 0) {
        printf("Error on write\n");
        exit(-1);
      }
    }
  }

  if (context_CHG != NULL) {
    num_items = array_list_size(context_CHG);

    for (int i = num_items - 1; i >= 0; i--) {
      metil_data = (metil_data_t *)array_list_get(i, context_CHG);
      file_error = fprintf(CHG, "%s\t%c\t%i\t%lu\t%c\t%i\n",
                          metil_data->query_name, metil_data->status,
                          metil_data->chromosome, metil_data->start,
                          metil_data->context, metil_data->strand);
      metil_data_free(metil_data);
      
      if (file_error < 0) {
        printf("Error on write\n");
        exit(-1);
      }
    }
  }

  if (context_CHH != NULL) {
    num_items = array_list_size(context_CHH);

    for (int i = num_items - 1; i >= 0; i--) {
      metil_data = (metil_data_t *)array_list_get(i, context_CHH);
      file_error = fprintf(CHH, "%s\t%c\t%i\t%lu\t%c\t%i\n",
                          metil_data->query_name, metil_data->status,
                          metil_data->chromosome, metil_data->start,
                          metil_data->context, metil_data->strand);
      metil_data_free(metil_data);
      
      if (file_error < 0) {
        printf("Error on write\n");
        exit(-1);
      }
    }
  }
 
  if (context_MUT != NULL) {
    num_items = array_list_size(context_MUT);
    for (int i = num_items - 1; i >= 0; i--) {
      metil_data = (metil_data_t *)array_list_get(i, context_MUT);
      file_error = fprintf(MUT, "%s\t%c\t%i\t%lu\t%c\t%i\n",
                          metil_data->query_name, metil_data->status,
                          metil_data->chromosome, metil_data->start,
                          metil_data->context, metil_data->strand);
      metil_data_free(metil_data);
      
      if (file_error < 0) {
        printf("Error on write\n");
        exit(-1);
      }
    }
  }

  if (context_CpG) array_list_free(context_CpG, NULL);
  if (context_CHG) array_list_free(context_CHG, NULL);
  if (context_CHH) array_list_free(context_CHH, NULL);
  if (context_MUT) array_list_free(context_MUT, NULL);
  if (bs_context)  bs_context_free(bs_context);
}

//------------------------------------------------------------------------------------

void metil_data_init(metil_data_t *metil_data, char *query, char status, int chromosome, size_t start, char context, int strand, int zone) {
  if (metil_data == NULL)
    metil_data = (metil_data_t *)malloc(sizeof(metil_data_t));

  metil_data->query_name = strdup(query);
  metil_data->status = status;
  metil_data->chromosome = chromosome;
  metil_data->start = start;
  metil_data->context = context;
  metil_data->strand = strand;
  metil_data->zone = zone;
}

//------------------------------------------------------------------------------------

void metil_data_free(metil_data_t *metil_data) {
  if (metil_data != NULL) {
    free(metil_data->query_name);
    free(metil_data);
  }
}

//------------------------------------------------------------------------------------

void postprocess_bs(char *query_name, char status, size_t chromosome, size_t start, char context, int strand, int region,
		    array_list_t *list) {

  metil_data_t *metil_data = (metil_data_t *) malloc(sizeof(metil_data_t));

  metil_data_init(metil_data, query_name, status, chromosome, start, context, strand, region);
  array_list_insert(metil_data, list);
}

//------------------------------------------------------------------------------------

void search_methylation(int c, size_t init, size_t end, unsigned long long **values, char *read, bs_context_t *bs_context, char strand, int type, char *query_name){
  size_t init_num, end_num;
  int i, init_num_pos, end_num_pos;
  unsigned long long tmp, res;
  size_t j, pos;
  size_t algo = 0;

  int write_file = 1;
  int elem = (sizeof(unsigned long long) << 2);

  init_num = init / elem;
  init_num_pos = init % elem;
  end_num = end / elem;
  end_num_pos = end % elem;
  char c1, c2;

  if (type == 0) {
    c1 = 'C'; c2 = 'T';
  } else {
    c1 = 'G'; c2 = 'A';
  }

  // Recorrer los valores de derecha a izquierda
  tmp  = values[c][end_num];

  // Descartar los pares de bits menos significativos hasta llegar al valor donde termina la cadena
  for (i = elem; i > end_num_pos; i--) {
    tmp = tmp >> 2;
  }


  // Recorrer los pares de bits comprobando el contexto del ultimo numero
  for (i = end_num_pos, pos = 0; i >= 0; i--, pos++) {
    search_gen();
  }

  // Recorrer los numeros entre el primer y ultimo valor de la cadena
  for (j = end_num - 1; j > init_num; j--) {
    tmp  = values[c][j];
    
    // Recorrer los elem pares de bits (elem letras del genoma) del numero en cuestion
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
    search_gen();
  }
  
  // Recorrer los n ultimos pares de bits del primer numero (inicio de la cadena)
  tmp  = values[c][init_num];
  
  for (i = elem; i >= init_num_pos; i--, pos++) {
    search_gen();
  }
  
  return;
}

//------------------------------------------------------------------------------------

int encode_context(char* filename, char* directory) {
  printf("Init Genome Compresion\n");

  FILE *f1, *f2, *f3, *f4;
  unsigned long long value, value2;
  size_t size[50] = {0};
  size_t size2;
  int chromosome, i, cont, cont2;
  char *line1, *line2;
  char *tmp;
  size_t contador = 100000000;

  int elem = sizeof(unsigned long long) << 2;

  size2 = strlen(directory);
  tmp = (char *)malloc((size2 + 40) * sizeof(char));
  line1 = (char *)malloc(512 * sizeof(char));
  line2 = (char *)malloc(512 * sizeof(char));

  f1 = fopen (filename, "r");

  if (f1 == NULL) {
    perror("No se puede abrir el fichero de entrada");
    return -1;
  }

  sprintf(tmp, "%s/Genome_context_CT.bin", directory);
  f2 = fopen (tmp, "wb");

  if (f2 == NULL) {
    perror("No se puede abrir el fichero de contexto CT");
    return -1;
  }

  sprintf(tmp, "%s/Genome_context_GA.bin", directory);
  f3 = fopen (tmp, "wb");

  if (f3 == NULL) {
    perror("No se puede abrir el fichero de contexto GA");
    return -1;
  }

  sprintf(tmp, "%s/Genome_context_size.txt", directory);
  f4 = fopen (tmp, "w");

  if (f4 == NULL) {
    perror("No se puede abrir el fichero de tamaños");
    return -1;
  }
  
  chromosome = 0;
  size[chromosome] = 0;
  cont  = 0;
  cont2 = 0;

  // Descartar la primera linea
  do {
    fgets(line1, 512, f1);
  } while(line1[0] == '>');
  
  
  while (fgets(line2, 512, f1) != NULL && contador) {
    contador--;

    if (line1[0] == '>') {
      size[chromosome]++;
      printf("chromosomes = %2i, size = %10lu\n", chromosome, size[chromosome]);
      chromosome++;

      for (; cont > 0 && cont <= elem; cont++) {
	      value = value << 2;
      }

      for (; cont2 > 0 && cont2 <= elem; cont2++) {
	      value2 = value2 << 2;
      }

      fwrite(&value,  sizeof(unsigned long long), 1, f2);
      fwrite(&value2, sizeof(unsigned long long), 1, f3);

      cont  = 0;
      cont2 = 0;
    } else {
      size2 = strlen(line1);

      // Value for C->T conversion
      for (i = 0; i < size2 - 2; i++, cont++) {
        if (line1[i] == 'C') {
          if (line1[i + 1] == 'G') {
            value += 1;
          } else {
            if (line1[i + 2] == 'G') {
              value += 2;
            } else {
              value += 3;
            }
          }
        }
        if (cont == elem) {
          cont = 0;
          size[chromosome]++;
          fwrite(&value, sizeof(unsigned long long), 1, f2);
        } else {
          value = value << 2;
        }
      }
      
      if (line1[size2 - 2] == 'C') {
        if (line1[size2 - 1] == 'G') {
          value += 1;
        } else {
          if (line2[0] == 'G') {
            value += 2;
          } else {
            value += 3;
          }
        }
      }

      if (cont == elem) {
        cont = 0;
        size[chromosome]++;
        fwrite(&value, sizeof(unsigned long long), 1, f2);
      } else {
	      value = value << 2;
      }

      cont++;

      if (line1[size2 - 1] == 'C') {
        if (line2[0] == 'G') {
          value += 1;
        } else {
          if (line2[1] == 'G') {
            value += 2;
          } else {
            value += 3;
          }
        }
      }

      if (cont == elem) {
        cont = 0;
        size[chromosome]++;
        fwrite(&value, sizeof(unsigned long long), 1, f2);
      } else {
	      value = value << 2;
      }
      cont++;
      // End value for C->T conversion

      // Value for G->A conversion
      for (i = 2; i < size2; i++, cont2++) {
        if (line1[i] == 'G') {
          if (line1[i - 1] == 'C') {
            value2 += 1;
          } else {
            if (line1[i - 2] == 'C') {
              value2 += 2;
            } else {
              value2 += 3;
            }
          }
        }
        if (cont2 == elem) {
          cont2 = 0;
          fwrite(&value2, sizeof(unsigned long long), 1, f3);
        } else {
          value2 = value2 << 2;
        }
      }
      
      if (line2[0] == 'G') {
        if (line1[size2 - 1] == 'C') {
          value2 += 1;
        } else {
          if (line1[size2 - 2] == 'C') {
            value2 += 2;
          } else {
            value2 += 3;
          }
        }
      }

      if (cont2 == elem) {
        cont2 = 0;
        fwrite(&value2, sizeof(unsigned long long), 1, f3);
      } else {
	      value2 = value2 << 2;
      }

      cont2++;

      if (line2[1] == 'G') {
        if (line2[0] == 'C') {
          value2 += 1;
        } else {
          if (line1[size2 - 1] == 'C') {
            value2 += 2;
          } else {
            value2 += 3;
          }
        }
      }

      if (cont2 == elem) {
        cont2 = 0;
        fwrite(&value2, sizeof(unsigned long long), 1, f3);
      } else {
	      value2 = value2 << 2;
      }

      cont2++;
      // End value for G->A conversion
    }

    free(line1);
    line1 = strdup(line2);
  }

  fprintf(f4, "%i\n", chromosome);

  for (i = 0; i < chromosome; i++) {
    fprintf(f4, "%lu\n", size[i]);
  }

  free(tmp);
  free(line1);
  free(line2);
  fclose(f1);
  fclose(f2);
  fclose(f3);
  fclose(f4);

  printf("End Genome Compresion\n");
  return 0;
}

//------------------------------------------------------------------------------------

int load_encode_context(char* directory, unsigned long long **valuesCT, unsigned long long **valuesGA) {
  FILE *f1, *f2, *f3;
  size_t size, size2 = strlen(directory);
  char *tmp = (char *)malloc((size2 + 50) * sizeof(char));
  int ret, chromosome, i;

  sprintf(tmp, "%s/Genome_context_size.txt", directory);
  f1 = fopen (tmp, "r");

  if (f1 == NULL) {
    perror("No se puede abrir el fichero de tamaños");
    ret = -1;
    goto end1;
  }

  sprintf(tmp, "%s/Genome_context_CT.bin", directory);
  f2 = fopen (tmp, "rb");
  
  if (f2 == NULL) {
    perror("No se puede abrir el fichero de contexto CT");
    ret = -1;
    goto end2;
  }

  sprintf(tmp, "%s/Genome_context_GA.bin", directory);
  f3 = fopen (tmp, "rb");
  
  if (f3 == NULL) {
    perror("No se puede abrir el fichero de contexto GA");
    ret = -1;
    goto end3;
  }

  fscanf(f1, "%i\n", &chromosome);

  for (i = 0; i < chromosome; i++) {
    fscanf(f1, "%lu\n", &size);

    valuesCT[i] = (unsigned long long *)calloc(size, sizeof(unsigned long long));
    fread (valuesCT[i], sizeof(unsigned long long), size, f2);

    valuesGA[i] = (unsigned long long *)calloc(size, sizeof(unsigned long long));
    fread (valuesGA[i], sizeof(unsigned long long), size, f3);
  }

  ret = chromosome;

  fclose(f3);
end3:
  fclose(f2);
end2:
  fclose(f1);
end1:
  free(tmp);

  return ret;
}
