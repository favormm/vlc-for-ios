/*
 *     Generated by class-dump 3.1.1.
 *
 *     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2006 by Steve Nygard.
 */

#import "NSObject.h"

@interface BRRenderPixelFormat : NSObject
{
    struct _CGLPixelFormatObject *_pixFormat;
}

+ (id)doubleBuffered;
+ (id)doubleBufferedWithDisplay:(unsigned int)fp8;
+ (id)formatWithAttributes:(const int *)fp8;
- (id)initWithAttributes:(const int *)fp8;
- (void)dealloc;
- (struct _CGLPixelFormatObject *)CGLPixelFormat;

@end

