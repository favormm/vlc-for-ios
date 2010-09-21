//
//  AQGridViewAnimatorItem.h
//  Kobov3
//
//  Created by Jim Dovey on 10-06-29.
//  Copyright 2010 Kobo Inc. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface AQGridViewAnimatorItem : NSObject
{
    UIView * animatingView;
    NSUInteger index;
}
+ (AQGridViewAnimatorItem *) itemWithView: (UIView *) aView index: (NSUInteger) anIndex;

@property (nonatomic, retain) UIView * animatingView;	// probably an AQGridViewCell, maybe a UIImageView
@property (nonatomic, assign) NSUInteger index;			// the DESTINATION index -- use NSNotFound if this is being deleted

@end
