/*****************************************************************************/
/* Begin #include statements */

/* End #include statements */
/*****************************************************************************/


/*****************************************************************************/
/* Begin function prototypes */

int is_same(char *string1, char *string2);
void strip_comment(char *string);
int is_comment(char *string);
ChannelLabel *channel_label_append(ChannelLabel *head, char *channel, char *label);
int get_label(char *line, char *expect, char *value1, size_t value1_len, 
	      char *value2, size_t value2_len, char quote1, char quote2);
MixerInfo *create_mixer_info(Mixer *mixer, int num, unsigned int flags);
CBData *create_cb_data(Group *group, void *handle, int element, int index);

/* End function prototypes */
/*****************************************************************************/

/*****************************************************************************/
/* Begin #define statements */


/* End #define statements */
/*****************************************************************************/

/*****************************************************************************/
/* Begin Macros */

#define EAZERO(S, L) S[L-1] = '\0';

/* End Macros */
/*****************************************************************************/

