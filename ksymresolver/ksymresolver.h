/*
 * Created 190112 lynnl
 */
#ifndef KSYMRESOLVER_H
#define KSYMRESOLVER_H

/**
 * Resolve a kernel symbol address
 * @param name          symbol name(must begin with _)
 * @return              NULL if not found
 */
void *resolve_ksymbol(const char *);

#endif  /* KSYMRESOLVER_H */

