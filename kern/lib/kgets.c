#include <types.h>
#include <lib.h>

/*
 * Do a backspace in typed input.
 * We overwrite the current character with a space in case we're on
 * a terminal where backspace is nondestructive.
 */
void
backsp(void)
{
	putch('\b');
	putch(' ');
	putch('\b');
}

/*
 * Read a string off the console. Support a few of the more useful
 * common control characters. Do not include the terminating newline
 * in the buffer passed back.
 */
void
kgets(char *buf, size_t maxlen)
{
	size_t pos = 0;
	int ch;

	while (1) {
		ch = getch();
		if (ch=='\n' || ch=='\r') {
			putch('\n');
			break;
		}

		/* Only allow the normal 7-bit ascii */
		if (ch>=32 && ch<127 && pos < maxlen-1) {
			putch(ch);
			buf[pos++] = ch;
		}
		else if ((ch=='\b' || ch==127) && pos>0) {
			/* backspace */
			backsp();
			pos--;
		}
		else if (ch==3) {
			/* ^C - return empty string */
			putch('^');
			putch('C');
			putch('\n');
			pos = 0;
			break;
		}
		else if (ch==18) {
			/* ^R - reprint input */
			buf[pos] = 0;
			kprintf("^R\n%s", buf);
		}
		else if (ch==21) {
			/* ^U - erase line */
			while (pos > 0) {
				backsp();
				pos--;
			}
		}
		else if (ch==23) {
			/* ^W - erase word */
			while (pos > 0 && buf[pos-1]==' ') {
				backsp();
				pos--;
			}
			while (pos > 0 && buf[pos-1]!=' ') {
				backsp();
				pos--;
			}
		}
		else {
			beep();
		}
	}

	buf[pos] = 0;
}

/* 
 * Modified verion of kgets, made for sys_read.
 * Take input off of console and store in kbuf 
 * This is essentially a slightly modified version of kgets
 * Modified it to store carriage returns. Original kgets also did this weird printing thing, got rid of that
 * Note that the kbuf[kbuflen-1] is always the null character, kbuflen is the number of bytes that the buffer can hold
 */

void kgets_sys_read(char *kbuf, int kbuflen)
{
	int kbuf_pos = 0;
	char ch;

	while(1) 
	{
		/* Check to see if buffer is filled */
		if( !(kbuf_pos < kbuflen-1) )
		{
			break;
		}

		ch = getch();

		/* new line and carriage returns */
		if(ch=='\n' || ch=='\r') 
		{
			kbuf[kbuf_pos] = ch;
			putch('\n');
			break;
		}
		/* Only allow the normal 7-bit ascii */
		else if (ch>=32 && ch<127) {
			kbuf[kbuf_pos] = ch;
			kbuf_pos++;
		}
		else if (ch=='\b' || ch==127) {
			/* backspace */
			backsp();
			kbuf_pos--;
		}
		else if (ch==3) {
			/* ^C - return empty string */
			putch('^');
			putch('C');
			putch('\n');
			kbuf_pos = 0;
			break;
		}
		else if (ch==18) {
			/* ^R - reprint input */
			kbuf[kbuf_pos] = 0;
			kprintf("^R\n%s", kbuf);
		}
		else if (ch==21) {
			/* ^U - erase line */
			while (kbuf_pos > 0) {
			backsp();
			kbuf_pos--;
			}
		}
		else if (ch==23) {
			/* ^W - erase word */
			while (kbuf_pos > 0 && kbuf[kbuf_pos-1]==' ') {
				backsp();
				kbuf_pos--;
			}	
			while (kbuf_pos > 0 && kbuf[kbuf_pos-1]!=' ') {
				backsp();
				kbuf_pos--;
			}
		}
	}
	kbuf[kbuflen-1] = '\0'; // NULL terminate
	return;
}
